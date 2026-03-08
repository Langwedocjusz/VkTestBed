#version 450

#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "../common/VertexNaive.glsl"
//#include "../common/VertexCompressed.glsl"

layout(location = 0) out VertexData {
    vec2 TexCoord;
} OutData;

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

// Using adjugate transform matrix to transform normal vectors:
// Credit to Inigo Quilez: https://www.shadertoy.com/view/3s33zj
mat3 adjugate(mat4 m)
{
    return mat3(cross(m[1].xyz, m[2].xyz), 
                cross(m[2].xyz, m[0].xyz), 
                cross(m[0].xyz, m[1].xyz));
}

void main() {
    const float outlineFactor = 0.01;

    Vertex vert = PushConstants.VertBuff.Vertices[gl_VertexIndex];

    vec3 position3 = GetPosition(vert);
    vec2 texcoord = GetTexCoord(vert);
    vec3 normal = GetNormal(vert);

    texcoord = 2.0 * texcoord - 1.0;
    texcoord *= PushConstants.TexExtent;
    texcoord += PushConstants.TexCenter;

    normal = normalize(adjugate(PushConstants.Model) * normal);

    vec4 position = vec4(position3, 1.0);
    position = PushConstants.Model * position;  
    position.xyz += outlineFactor * normal;
    position = Ubo.CameraViewProjection * position;

    gl_Position = position;
    
    OutData.TexCoord = texcoord;
}