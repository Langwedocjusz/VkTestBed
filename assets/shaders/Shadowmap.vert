#version 450

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;

layout(location = 0) out vec2 texCoord;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 CameraViewProjection;
    mat4 LightViewProjection;
} Ubo;

layout(push_constant) uniform constants {
    vec4 OtherThings;
    mat4 Transform;
} PushConstants;

void main() {
    mat4 MVP = Ubo.LightViewProjection * PushConstants.Transform;

    texCoord = aTexCoord;
    gl_Position = MVP * vec4(aPosition, 1.0);
}