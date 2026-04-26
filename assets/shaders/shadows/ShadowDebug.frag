#version 450

layout(location = 0) out vec4 vOutColor;

layout(push_constant) uniform PushConstants {
    mat4 ViewProj;
    vec4 Color;
} uPushConstants;

void main()
{
    vOutColor = uPushConstants.Color;
}