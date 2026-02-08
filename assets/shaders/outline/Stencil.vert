#version 450

#extension GL_EXT_scalar_block_layout : require

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aTangent;

layout(location = 0) out VertexData {
    vec2 TexCoord;
} OutData;

layout(scalar, set = 0, binding = 0) uniform DynamicUBOBlock {
    mat4 CameraViewProjection;
    mat4 LightViewProjection[3]; //TODO: Must be kept in-sync with shadowmap cascades
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

    OutData.TexCoord = aTexCoord;

    gl_Position = MVP * vec4(aPosition, 1.0);
}