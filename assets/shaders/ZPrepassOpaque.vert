#version 450

#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "common/VertexCompressed.glsl"

layout(scalar, set = 0, binding = 0) uniform DynamicUBOBlock {
    mat4 CameraViewProjection;
    mat4 LightViewProjection[3]; //TODO: Must be kept in-sync with shadowmap cascades
    float CascadeBounds[3];
    vec3 ViewPos;
    vec3 ViewFront;
    float DirectionalFactor;
    float EnvironmentFactor;
    float ShadowBiasMin;
    float ShadowBiasMax;
} Ubo;

layout(push_constant) uniform constants {
    mat4 Model;
    VertexBuffer VertBuff;
} PushConstants;

void main() {
    Vertex vert = PushConstants.VertBuff.Vertices[gl_VertexIndex];
    
    vec3 position = GetPosition(vert);

    mat4 MVP = Ubo.CameraViewProjection * PushConstants.Model;
    
    gl_Position = MVP * vec4(position, 1.0);
}