#include "render.h"
#include "coal/m_math.h"
#include "coal/util.h"
#include "tanto/r_geo.h"
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
#include <tanto/r_renderpass.h>
#include <tanto/v_command.h>
#include <vulkan/vulkan_core.h>

#define SPVDIR "./shaders/spv"

static Tanto_V_Image renderTargetDepth;

static VkRenderPass  renderpass;
static VkFramebuffer framebuffers[TANTO_FRAME_COUNT];
static VkPipeline    mainPipeline;

#define MAX_PRIM_COUNT 16

static Tanto_V_BufferRegion cameraBuffers[TANTO_FRAME_COUNT];
static Tanto_V_BufferRegion xformsBuffers[TANTO_FRAME_COUNT];

static uint8_t cameraNeedUpdate;
static uint8_t framesNeedUpdate;
static uint8_t xformsNeedUpdate;

typedef struct {
    Mat4 xform[MAX_PRIM_COUNT];
} Xforms;

typedef struct {
    Mat4 view;
    Mat4 proj;
} Camera;

typedef struct {
    Vec4 vec4_0;
    Vec4 vec4_1;
} PushConstant;

static const Tanto_S_Scene* scene;

static VkDescriptorSetLayout descriptorSetLayouts[TANTO_MAX_DESCRIPTOR_SETS];
static Tanto_R_Description   description[TANTO_FRAME_COUNT];

static VkPipelineLayout pipelineLayout;

typedef enum {
    PIPE_LAYOUT_MAIN,
} PipelineLayoutId;

typedef enum {
    DESC_SET_MAIN,
} DescriptorSetId;

// declarations for overview and navigation
static void initAttachments(void);
static void initRenderPass(void);
static void initFramebuffers(void);
static void initDescriptorSetsAndPipelineLayouts(void);
static void initPipelines(void);
static void updateDescriptors(void);
static void mainRender(const VkCommandBuffer cmdBuf, const uint32_t frameIndex);
static void updateRenderCommands(const uint32_t frameIndex);
static void onSwapchainRecreate(void);
static void updateCamera(uint32_t index);
static void updateXform(uint32_t frameIndex, uint32_t primIndex);
static void syncScene(void);
void r_InitRenderer(void);
void r_Render(void);
void r_BindScene(const Tanto_S_Scene* pScene);
void r_CleanUp(void);

// TODO: we should implement a way to specify the offscreen renderpass format at initialization
static void initAttachments(void)
{
    renderTargetDepth = tanto_v_CreateImage(
        TANTO_WINDOW_WIDTH, TANTO_WINDOW_HEIGHT,
        tanto_r_GetDepthFormat(),
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_SAMPLE_COUNT_1_BIT);
}

static void initRenderPass(void)
{
    tanto_r_CreateRenderPass_ColorDepth(VK_ATTACHMENT_LOAD_OP_CLEAR, 
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
            tanto_r_GetSwapFormat(), tanto_r_GetDepthFormat(), 
            &renderpass);
}

static void initFramebuffers(void)
{
    for (int i = 0; i < TANTO_FRAME_COUNT; i++) 
    {
        Tanto_R_Frame* frame = tanto_r_GetFrame(i);
        const VkImageView attachments[] = {
            frame->swapImage.view, renderTargetDepth.view
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

static void initDescriptorSetsAndPipelineLayouts(void)
{
    const Tanto_R_DescriptorSetInfo descriptorSets[] = {{
        .bindingCount = 2,
        .bindings = {{
            .descriptorCount = 1,
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
        },{
            .descriptorCount = 1,
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
        }}
    }};

    const uint8_t descSetCount = TANTO_ARRAY_SIZE(descriptorSets);
    tanto_r_CreateDescriptorSetLayouts(descSetCount, descriptorSets, descriptorSetLayouts);

    for (int i = 0; i < TANTO_FRAME_COUNT; i++) 
    {
        tanto_r_CreateDescriptorSets(descSetCount, descriptorSets, descriptorSetLayouts, &description[i]);
    }

    const VkPushConstantRange pcRangeVert = {
        .offset = 0,
        .size = sizeof(Vec4),
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
    };

    const VkPushConstantRange pcRangeFrag = {
        .offset = sizeof(Vec4),
        .size = sizeof(Vec4),
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };

    const VkPushConstantRange ranges[] = {pcRangeVert, pcRangeFrag};

    const Tanto_R_PipelineLayoutInfo pipeLayoutInfos[] = {{
        .descriptorSetCount = 1, 
        .descriptorSetLayouts = descriptorSetLayouts,
        .pushConstantCount = TANTO_ARRAY_SIZE(ranges),
        .pushConstantsRanges = ranges
    }};

    tanto_r_CreatePipelineLayouts(1, pipeLayoutInfos, &pipelineLayout);
}

static void initPipelines(void)
{
    const Tanto_R_GraphicsPipelineInfo graphPipeInfo = {
        .renderPass = renderpass, 
        .layout     = pipelineLayout,
        .sampleCount = VK_SAMPLE_COUNT_1_BIT,
        //.polygonMode = VK_POLYGON_MODE_LINE,
        .frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .vertexDescription = tanto_r_GetVertexDescription3D_3Vec3(),
        .vertShader = SPVDIR"/template-vert.spv",
        .fragShader = SPVDIR"/template-frag.spv"
    };

    tanto_r_CreateGraphicsPipelines(1, &graphPipeInfo, &mainPipeline);
}

static void updateDescriptors(void)
{
    for (int i = 0; i < TANTO_FRAME_COUNT; i++) 
    {
        // camera creation
        cameraBuffers[i] = tanto_v_RequestBufferRegion(sizeof(Camera),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, TANTO_V_MEMORY_HOST_TYPE);
        Camera* camera = (Camera*)(cameraBuffers[i].hostData);

        Mat4 view = m_Ident_Mat4();

        camera->view = m_Translate_Mat4((Vec3){0, 0, -1}, &view);
        camera->proj = m_BuildPerspective(0.001, 100);

        // xforms creation
        xformsBuffers[i] = tanto_v_RequestBufferRegion(sizeof(Xforms), 
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, TANTO_V_MEMORY_HOST_TYPE);

        Xforms* modelXform = (Xforms*)(xformsBuffers[i].hostData);

        for (int i = 0; i < MAX_PRIM_COUNT; i++) 
        {
            modelXform->xform[i] = m_Ident_Mat4();
        }

        VkDescriptorBufferInfo camInfo = {
            .buffer = cameraBuffers[i].buffer,
            .offset = cameraBuffers[i].offset,
            .range  = cameraBuffers[i].size
        };

        VkDescriptorBufferInfo xformInfo = {
            .buffer = xformsBuffers[i].buffer,
            .offset = xformsBuffers[i].offset,
            .range  = xformsBuffers[i].size
        };

        VkWriteDescriptorSet writes[] = {{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstArrayElement = 0,
            .dstSet = description[i].descriptorSets[DESC_SET_MAIN],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &camInfo
        },{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstArrayElement = 0,
            .dstSet = description[i].descriptorSets[DESC_SET_MAIN],
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &xformInfo
        }};

        vkUpdateDescriptorSets(device, TANTO_ARRAY_SIZE(writes), writes, 0, NULL);
    }
}

static void mainRender(const VkCommandBuffer cmdBuf, const uint32_t frameIndex)
{
    VkClearValue clearValueColor = {0.002f, 0.001f, 0.009f, 1.0f};
    VkClearValue clearValueDepth = {1.0, 0};

    VkClearValue clears[] = {clearValueColor, clearValueDepth};

    const VkRenderPassBeginInfo rpassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .clearValueCount = 2,
        .pClearValues = clears,
        .renderArea = {{0, 0}, {TANTO_WINDOW_WIDTH, TANTO_WINDOW_HEIGHT}},
        .renderPass =  renderpass,
        .framebuffer = framebuffers[frameIndex],
    };

    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, mainPipeline);

    vkCmdBindDescriptorSets(
        cmdBuf, 
        VK_PIPELINE_BIND_POINT_GRAPHICS, 
        pipelineLayout,
        0, 1, &description[frameIndex].descriptorSets[DESC_SET_MAIN],
        0, NULL);

    vkCmdBeginRenderPass(cmdBuf, &rpassInfo, VK_SUBPASS_CONTENTS_INLINE);

    Vec4 debugColor = (Vec4){1, 0, 0.5};
    vkCmdPushConstants(cmdBuf, pipelineLayout, 
            VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Vec4), &debugColor);

    assert(scene->lightCount > 0);

    vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Vec4), sizeof(Vec4), &scene->lights[0]); 

    assert(sizeof(Vec4) == sizeof(Tanto_S_Matrial));
    assert(scene->primCount < MAX_PRIM_COUNT);

    for (int p = 0; p < scene->primCount; p++) 
    {
        vkCmdPushConstants(cmdBuf, pipelineLayout, 
                VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Vec4), &scene->materials[p]);
        tanto_r_DrawPrim(cmdBuf, &scene->prims[p]);
    }

    vkCmdEndRenderPass(cmdBuf);
}

static void updateRenderCommands(const uint32_t frameIndex)
{
    Tanto_R_Frame* frame = tanto_r_GetFrame(frameIndex);
    vkResetCommandPool(device, frame->command.commandPool, 0);
    VkCommandBufferBeginInfo cbbi = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    V_ASSERT( vkBeginCommandBuffer(frame->command.commandBuffer, &cbbi) );

    mainRender(frame->command.commandBuffer, frameIndex);

    V_ASSERT( vkEndCommandBuffer(frame->command.commandBuffer) );
}

static void onSwapchainRecreate(void)
{
    r_CleanUp();
    initAttachments();
    initPipelines();
    initFramebuffers();
    framesNeedUpdate = TANTO_FRAME_COUNT;
}

static void updateCamera(uint32_t index)
{
    //printf("Updating Camera: index %d\n", index);
    //printf("camera\n");
    //coal_PrintMat4(&scene->camera.xform);
    const Mat4 proj = m_BuildPerspective(0.001, 100);
    const Mat4 view = m_Invert4x4(&scene->camera.xform);
    Camera* uboCam = (Camera*)cameraBuffers[index].hostData;
    uboCam->view = view;
    uboCam->proj = proj;
}

static void updateXform(uint32_t frameIndex, uint32_t primIndex)
{
    Xforms* xforms = (Xforms*)xformsBuffers[frameIndex].hostData;
    xforms->xform[primIndex] = scene->xforms[primIndex];
}

static void syncScene(void)
{
    if (scene->dirt)
    {
        if (scene->dirt & TANTO_S_CAMERA_BIT)
        {
            cameraNeedUpdate = TANTO_FRAME_COUNT;
        }
        if (scene->dirt & TANTO_S_LIGHTS_BIT)
        {
            framesNeedUpdate = TANTO_FRAME_COUNT;
        }
        if (scene->dirt & TANTO_S_XFORMS_BIT)
            xformsNeedUpdate = TANTO_FRAME_COUNT;
    }
    if (cameraNeedUpdate)
    {
        uint32_t i = tanto_r_GetCurrentFrameIndex();
        tanto_r_WaitOnFrame(i);
        updateCamera(i);
        cameraNeedUpdate--;
    }
    if (xformsNeedUpdate)
    {
        uint32_t f = tanto_r_GetCurrentFrameIndex();
        tanto_r_WaitOnFrame(f);
        for (int i = 0; i < scene->primCount; i++) 
        {
            printf("Updating xform for frame %d prim %d\n", f, i);
            updateXform(f, i);
        }
        xformsNeedUpdate--;
    }
    if (framesNeedUpdate)
    {
        uint32_t i = tanto_r_GetCurrentFrameIndex();
        tanto_r_WaitOnFrame(i);
        updateRenderCommands(i);
        framesNeedUpdate--;
    }
}

void r_InitRenderer(void)
{
    cameraNeedUpdate = TANTO_FRAME_COUNT;
    xformsNeedUpdate = TANTO_FRAME_COUNT;
    framesNeedUpdate = TANTO_FRAME_COUNT;

    initAttachments();
    initRenderPass();
    initFramebuffers();
    initDescriptorSetsAndPipelineLayouts();
    initPipelines();
    updateDescriptors();

    tanto_r_RegisterSwapchainRecreationFn(onSwapchainRecreate);
}

void r_Render(void)
{
    syncScene();
    tanto_r_SubmitFrame();
}

void r_BindScene(const Tanto_S_Scene* pScene)
{
    printf("Renderer: Scene bound.\n");
    scene = pScene;
}

void r_CleanUp(void)
{
    for (int i = 0; i < TANTO_FRAME_COUNT; i++) 
    {
        vkDestroyFramebuffer(device, framebuffers[i], NULL);
    }
    tanto_v_FreeImage(&renderTargetDepth);
    vkDestroyPipeline(device, mainPipeline, NULL);
}
