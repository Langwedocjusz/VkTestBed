#version 450

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;

layout(location = 0) out vec3 fragColor;

layout( push_constant ) uniform constants
{
    mat4 Transform;
} PushConstants;

void main() {
    gl_Position = PushConstants.Transform * vec4(aPosition, 1.0);
    fragColor = aColor;
}