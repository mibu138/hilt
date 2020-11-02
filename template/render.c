#include "render.h"
#include "tanto/m_math.h"
#include "tanto/v_image.h"
#include "tanto/v_memory.h"
#include <memory.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <tanto/r_render.h>
#include <tanto/v_video.h>
#include <tanto/t_def.h>
#include <tanto/t_utils.h>
#include <tanto/r_pipeline.h>
#include <tanto/r_raytrace.h>
#include <tanto/r_renderpass.h>
#include <tanto/v_command.h>
#include <vulkan/vulkan_core.h>

#define SPVDIR "/home/michaelb/dev/hilt/template/shaders/spv"

static Tanto_V_Image renderTargetDepth;

static VkRenderPass  renderpass;
static VkFramebuffer framebuffers[TANTO_FRAME_COUNT];

static Tanto_V_BufferRegion uniformBufferRegion;

static Tanto_R_Primitive triangle;


typedef enum {
    R_PIPE_MAIN,
} R_PipelineId;

typedef enum {
    R_PIPE_LAYOUT_MAIN,
} R_PipelineLayoutId;

typedef enum {
    R_DESC_SET_MAIN,
} R_DescriptorSetId;

// TODO: we should implement a way to specify the offscreen renderpass format at initialization
static void initRenderTargets(void)
{
    renderTargetDepth = tanto_v_CreateImage(
        TANTO_WINDOW_WIDTH, TANTO_WINDOW_HEIGHT,
        depthFormat,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_SAMPLE_COUNT_1_BIT);
}

static void initRenderPass(void)
{
    const VkAttachmentDescription attachmentColor = {
        .flags = 0,
        .format = swapFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    const VkAttachmentDescription attachmentDepth = {
        .flags = 0,
        .format = depthFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    const VkAttachmentDescription attachments[] = {
        attachmentColor, attachmentDepth
    };

    VkAttachmentReference colorReference = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference depthReference = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    const VkSubpassDescription subpass = {
        .flags                   = 0,
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount    = 0,
        .pInputAttachments       = NULL,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &colorReference,
        .pResolveAttachments     = NULL,
        .pDepthStencilAttachment = &depthReference,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments    = NULL,
    };

    Tanto_R_RenderPassInfo rpi = {
        .attachmentCount = 2,
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
    };

    tanto_r_CreateRenderPass(&rpi, &renderpass);
}

static void initFrameBuffers(void)
{
    for (int i = 0; i < TANTO_FRAME_COUNT; i++) 
    {
        const VkImageView attachments[] = {
            frames[i].swapImage.view, renderTargetDepth.view
        };

        const VkFramebufferCreateInfo fbi = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .renderPass = renderpass,
            .attachmentCount = 2,
            .pAttachments = attachments,
            .width = TANTO_WINDOW_WIDTH,
            .height = TANTO_WINDOW_HEIGHT,
            .layers = 1,
        };

        V_ASSERT( vkCreateFramebuffer(device, &fbi, NULL, &framebuffers[i]) );
    }
}

static void initPipelines(void)
{
    const Tanto_R_DescriptorSet descriptorSets[] = {{
        .id = R_DESC_SET_MAIN,
        .bindingCount = 1,
        .bindings = {{
            .descriptorCount = 1,
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
        }}
    }};

    const Tanto_R_PipelineLayout pipelayouts[] = {{
        .id = R_PIPE_LAYOUT_MAIN, 
        .descriptorSetCount = 1, 
        .descriptorSetIds = {R_DESC_SET_MAIN},
        .pushConstantCount = 0,
        .pushConstantsRanges = {}
    }};

    const Tanto_R_PipelineInfo pipeInfos[] = {{
        .id       = R_PIPE_MAIN,
        .type     = TANTO_R_PIPELINE_RASTER_TYPE,
        .layoutId = R_PIPE_LAYOUT_MAIN,
        .payload.rasterInfo = {
            .renderPass = renderpass, 
            .sampleCount = VK_SAMPLE_COUNT_1_BIT,
            .vertexDescription = tanto_r_GetVertexDescription3D_Simple(),
            .vertShader = SPVDIR"/template-vert.spv",
            .fragShader = SPVDIR"/template-frag.spv"
        }
    }};

    tanto_r_InitDescriptorSets(descriptorSets, TANTO_ARRAY_SIZE(descriptorSets));
    tanto_r_InitPipelineLayouts(pipelayouts, TANTO_ARRAY_SIZE(pipelayouts));
    tanto_r_InitPipelines(pipeInfos, TANTO_ARRAY_SIZE(pipeInfos));
}

// descriptors that do only need to have update called once and can be updated on initialization
static void updateStaticDescriptors(void)
{
    uniformBufferRegion = tanto_v_RequestBufferRegion(sizeof(UniformBuffer), 
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, TANTO_V_MEMORY_HOST_GRAPHICS_TYPE);
    memset(uniformBufferRegion.hostData, 0, sizeof(Parms));
    UniformBuffer* uboData = (UniformBuffer*)(uniformBufferRegion.hostData);

    Mat4 view = m_Ident_Mat4();
    view = m_Translate_Mat4((Vec3){0, 0, -1}, &view);

    uboData->matModel = m_Ident_Mat4();
    uboData->matView  = view;
    uboData->matProj  = m_BuildPerspective(0.001, 100);

    VkDescriptorBufferInfo uboInfo = {
        .buffer = uniformBufferRegion.buffer,
        .offset = uniformBufferRegion.offset,
        .range  = uniformBufferRegion.size
    };

    VkWriteDescriptorSet writes[] = {{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstArrayElement = 0,
        .dstSet = descriptorSets[R_DESC_SET_MAIN],
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &uboInfo
    }};

    vkUpdateDescriptorSets(device, TANTO_ARRAY_SIZE(writes), writes, 0, NULL);
}

static void updateDynamicDescriptors(void)
{
}

static void mainRender(const VkCommandBuffer* cmdBuf, const VkRenderPassBeginInfo* rpassInfo)
{
    vkCmdBindPipeline(*cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[R_PIPE_MAIN]);

    vkCmdBindDescriptorSets(
        *cmdBuf, 
        VK_PIPELINE_BIND_POINT_GRAPHICS, 
        pipelineLayouts[R_PIPE_LAYOUT_MAIN], 
        0, 1, &descriptorSets[R_DESC_SET_MAIN],
        0, NULL);

    vkCmdBeginRenderPass(*cmdBuf, rpassInfo, VK_SUBPASS_CONTENTS_INLINE);

    const VkBuffer vertBuffers[2] = {
        triangle.vertexRegion.buffer,
        triangle.vertexRegion.buffer
    };

    const VkDeviceSize attrOffsets[2] = {
        triangle.attrOffsets[0] + triangle.vertexRegion.offset,
        triangle.attrOffsets[1] + triangle.vertexRegion.offset,
    };

    vkCmdBindVertexBuffers(*cmdBuf, 0, 2, vertBuffers, attrOffsets);

    vkCmdBindIndexBuffer(*cmdBuf, triangle.indexRegion.buffer, 
            triangle.indexRegion.offset, TANTO_VERT_INDEX_TYPE);

    vkCmdDrawIndexed(*cmdBuf, triangle.indexCount, 1, 0, 0, 0);

    vkCmdEndRenderPass(*cmdBuf);
}

void r_InitRenderer()
{
    initRenderTargets();
    initRenderPass();
    initFrameBuffers();
    initPipelines();
    updateStaticDescriptors();

    triangle = tanto_r_CreateTriangle();
}

void r_UpdateRenderCommands(void)
{
    Tanto_R_Frame* frame = &frames[curFrameIndex];
    vkResetCommandPool(device, frame->commandPool, 0);
    VkCommandBufferBeginInfo cbbi = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    V_ASSERT( vkBeginCommandBuffer(frame->commandBuffer, &cbbi) );

    VkClearValue clearValueColor = {0.002f, 0.023f, 0.009f, 1.0f};
    VkClearValue clearValueDepth = {1.0, 0};

    VkClearValue clears[] = {clearValueColor, clearValueDepth};

    const VkRenderPassBeginInfo rpassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .clearValueCount = 2,
        .pClearValues = clears,
        .renderArea = {{0, 0}, {TANTO_WINDOW_WIDTH, TANTO_WINDOW_HEIGHT}},
        .renderPass =  renderpass,
        .framebuffer = framebuffers[curFrameIndex],
    };

    mainRender(&frame->commandBuffer, &rpassInfo);

    V_ASSERT( vkEndCommandBuffer(frame->commandBuffer) );
}

void r_CleanUp(void)
{
    for (int i = 0; i < TANTO_FRAME_COUNT; i++) 
    {
        vkDestroyFramebuffer(device, framebuffers[i], NULL);
    }
    tanto_v_DestroyImage(renderTargetDepth);
}
