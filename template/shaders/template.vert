#version 460

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 norm;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec3 outNormal;

layout(set = 0, binding = 0) uniform Camera {
    mat4 view;
    mat4 proj;
} camera;

layout(set = 0, binding = 1) uniform Model {
    mat4 xform;
} model;

layout(push_constant) uniform PushConstant {
    vec3 color;
    uint id;
} push;

void main()
{
    gl_Position = camera.proj * camera.view * model.xform * vec4(pos, 1.0);
    outColor = push.color.rgb;
    outNormal = norm;
}
