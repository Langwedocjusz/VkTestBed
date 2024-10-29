#version 450

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec3 aNormal;

layout(location = 0) out vec3 fragColor;

layout(binding = 0) uniform UniformBufferObject {
    mat4 ViewProjection;
} Ubo;

layout(push_constant) uniform constants {
    mat4 Transform;
} PushConstants;

void main() {
    mat4 MVP = Ubo.ViewProjection * PushConstants.Transform;

    gl_Position = MVP * vec4(aPosition, 1.0);
    fragColor = aColor;
}