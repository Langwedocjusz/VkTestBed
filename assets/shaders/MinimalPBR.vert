#version 450

#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "common/VertexNaive.glsl"
//#include "common/VertexCompressed.glsl"

layout(location = 0) out VertexData {
    vec2 TexCoord;
    vec3 Normal;
    vec4 Tangent;
    vec3 FragPos;
    float FragDistance;
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
    mat4 MVP = Ubo.CameraViewProjection * PushConstants.Model;
    
    Vertex vert = PushConstants.VertBuff.Vertices[gl_VertexIndex];
    
    vec3 position = GetPosition(vert);
    vec2 texcoord = GetTexCoord(vert);
    vec3 normal = GetNormal(vert);
    vec4 tangent = GetTangent(vert);

    gl_Position = MVP * vec4(position, 1.0);

    texcoord = 2.0 * texcoord - 1.0;
    texcoord *= PushConstants.TexExtent;
    texcoord += PushConstants.TexCenter;

    normal = normalize(adjugate(PushConstants.Model) * normal);
    
    vec3 tangent3 = vec3(PushConstants.Model * vec4(tangent.xyz, 0.0));
    tangent3 = normalize(tangent3);

    OutData.TexCoord = texcoord;
    OutData.Normal = normal;
    OutData.Tangent = vec4(tangent3, tangent.w);

    OutData.FragPos = vec3(PushConstants.Model * vec4(position, 1.0));
    OutData.FragDistance = length(OutData.FragPos - Ubo.ViewPos);
}