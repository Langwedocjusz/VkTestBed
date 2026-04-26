#version 450

#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "../common/VertexNaive.glsl"
//#include "../common/VertexCompressed.glsl"

layout(location = 0) out VertexData {
    vec2 TexCoord;
} OutData;

layout(scalar, set = 0, binding = 0) uniform CameraBlock {
    mat4 ViewProjection;
    vec3 Pos;
    vec3 Front;
} uCamera;

layout(push_constant) uniform PushConstants {
    mat4 Model;
    VertexBuffer VertBuff;
    vec2 TexCenter;
    vec2 TexExtent;
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
    const float outlineFactor = 0.01;

    Vertex vert = uPushConstants.VertBuff.Vertices[gl_VertexIndex];

    vec3 position3 = GetPosition(vert);
    vec2 texcoord = GetTexCoord(vert);
    vec3 normal = GetNormal(vert);

    texcoord = 2.0 * texcoord - 1.0;
    texcoord *= uPushConstants.TexExtent;
    texcoord += uPushConstants.TexCenter;

    normal = normalize(Adjugate(uPushConstants.Model) * normal);

    vec4 position = vec4(position3, 1.0);
    position = uPushConstants.Model * position;  
    position.xyz += outlineFactor * normal;
    position = uCamera.ViewProjection * position;

    OutData.TexCoord = texcoord;

    gl_Position = position;
}