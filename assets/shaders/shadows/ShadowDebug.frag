#version 450

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform constants {
    mat4 ViewProj;
    vec4 Color;
} PushConstants;

void main()
{
    outColor = PushConstants.Color;
}