#version 450

#extension GL_EXT_scalar_block_layout : require

layout(location = 0) in vec3 aPosition;

layout(scalar, set = 0, binding = 0) uniform DynamicUBOBlock {
    mat4 CameraViewProjection;
    mat4 LightViewProjection;
    vec3 ViewPos;
    float DirectionalFactor;
    float EnvironmentFactor;
    float ShadowBiasMin;
    float ShadowBiasMax;
} Ubo;

layout(push_constant) uniform constants {
    mat4 Model;
} PushConstants;

void main() {
    mat4 MVP = Ubo.CameraViewProjection * PushConstants.Model;
    gl_Position = MVP * vec4(aPosition, 1.0);
}