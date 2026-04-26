#version 450

#extension GL_GOOGLE_include_directive : require

#include "../common/VertexNaive.glsl"
//#include "../common/VertexCompressed.glsl"

layout(push_constant) uniform PushConstants {
    mat4         LightMVP;
    VertexBuffer VertBuff;
    vec2         TexCenter;
    vec2         TexExtent;
} uPushConstants;

void main() {
    Vertex vert = uPushConstants.VertBuff.Vertices[gl_VertexIndex];
    
    vec3 position = GetPosition(vert);

    gl_Position = uPushConstants.LightMVP * vec4(position, 1.0);
}