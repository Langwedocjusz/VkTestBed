#version 450

#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

//#include "common/VertexNaive.glsl"
#include "common/VertexCompressed.glsl"

layout(location = 0) out vec2 texCoord;

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
    vec2 TexCenter;
    vec2 TexExtent;
} PushConstants;

void main() {
    Vertex vert = PushConstants.VertBuff.Vertices[gl_VertexIndex];
    
    vec3 position = GetPosition(vert);
    vec2 texcoord = GetTexCoord(vert);

    texcoord = 2.0 * texcoord - 1.0;
    texcoord *= PushConstants.TexExtent;
    texcoord += PushConstants.TexCenter;

    mat4 MVP = Ubo.CameraViewProjection * PushConstants.Model;

    gl_Position = MVP * vec4(position, 1.0);

    texCoord = texcoord;
}