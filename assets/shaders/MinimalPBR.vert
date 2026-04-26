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
} vOutData;

layout(scalar, set = 0, binding = 0) uniform CameraBlock {
    mat4  ViewProjection;
    vec3  Pos;
    vec3  Front;
} uCamera;

layout(push_constant) uniform PushConstants {
    mat4         Model;
    VertexBuffer VertBuff;
    vec2         TexCenter;
    vec2         TexExtent;
} uPushConstants;

// Using adjugate transform matrix to transform normal vectors:
// Credit to Inigo Quilez: https://www.shadertoy.com/view/3s33zj
mat3 Adjugate(mat4 m)
{
    return mat3(cross(m[1].xyz, m[2].xyz), 
                cross(m[2].xyz, m[0].xyz), 
                cross(m[0].xyz, m[1].xyz));
}

void main() {
    mat4 MVP = uCamera.ViewProjection * uPushConstants.Model;
    
    Vertex vert = uPushConstants.VertBuff.Vertices[gl_VertexIndex];
    
    vec3 position = GetPosition(vert);
    vec2 texcoord = GetTexCoord(vert);
    vec3 normal   = GetNormal(vert);
    vec4 tangent  = GetTangent(vert);

    texcoord = 2.0 * texcoord - 1.0;
    texcoord *= uPushConstants.TexExtent;
    texcoord += uPushConstants.TexCenter;

    normal = normalize(Adjugate(uPushConstants.Model) * normal);
    
    vec3 tangent3 = vec3(uPushConstants.Model * vec4(tangent.xyz, 0.0));
    tangent3 = normalize(tangent3);

    vOutData.TexCoord = texcoord;
    vOutData.Normal   = normal;
    vOutData.Tangent  = vec4(tangent3, tangent.w);
    vOutData.FragPos  = vec3(uPushConstants.Model * vec4(position, 1.0));

    gl_Position = MVP * vec4(position, 1.0);
}