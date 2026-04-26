#version 450

#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "../common/VertexNaive.glsl"
//#include "../common/VertexCompressed.glsl"

layout(location = 0) out VertexData {
    vec2 TexCoord;
} OutData;

layout(push_constant) uniform PushConstants {
    mat4         MVP;
    VertexBuffer VertBuff;
    vec2         TexCenter;
    vec2         TexExtent;
    uint         ObjectId;
} uPushConstants;

void main() {
    Vertex vert = uPushConstants.VertBuff.Vertices[gl_VertexIndex];
    
    vec3 position = GetPosition(vert);
    vec2 texcoord = GetTexCoord(vert);

    texcoord = 2.0 * texcoord - 1.0;
    texcoord *= uPushConstants.TexExtent;
    texcoord += uPushConstants.TexCenter;

    OutData.TexCoord = texcoord;
    gl_Position = uPushConstants.MVP * vec4(position, 1.0);
}