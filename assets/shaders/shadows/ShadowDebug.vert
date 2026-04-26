#version 450

layout(location = 0) in vec3 vPosition;

layout(push_constant) uniform PushConstants {
    mat4 ViewProj;
    vec4 Color;
} uPushConstants;

void main()
{
    gl_Position = uPushConstants.ViewProj * vec4(vPosition, 1.0);
}