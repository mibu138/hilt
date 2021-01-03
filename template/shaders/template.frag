#version 460

layout(location = 0) in  vec3 inColor;
layout(location = 1) in  vec3 inNormal;

layout(location = 0) out vec4 outColor;

struct Light {
    vec3  dir;
    float power;
};

layout(push_constant) uniform PushConstant {
    layout(offset = 16) Light light;
} push;

void main()
{
    float ambient = 0.05;
    float illume = clamp(dot(-1 * push.light.dir, inNormal), 0, 1);
    outColor = vec4(inColor, 1) * clamp(illume + ambient, 0, 1) * push.light.power;
}
