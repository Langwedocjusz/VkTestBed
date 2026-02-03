#version 450

layout(location = 0) in vec4 aPosition;

layout(push_constant) uniform constants {
    mat4 ViewProj;
    vec4 Color;
} PushConstants;

void main()
{
    gl_Position = PushConstants.ViewProj * vec4(aPosition.xyz, 1.0);
}