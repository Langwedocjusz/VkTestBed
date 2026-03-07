#version 450

#extension GL_GOOGLE_include_directive : require

#include "../common/VertexCompressed.glsl"

layout(push_constant) uniform constants {
    mat4 LightMVP;
    VertexBuffer VertBuff;
    vec2 TexCenter;
    vec2 TexExtent;
} PushConstants;

void main() {
    Vertex vert = PushConstants.VertBuff.Vertices[gl_VertexIndex];
    
    vec3 position = GetPosition(vert);

    gl_Position = PushConstants.LightMVP * vec4(position, 1.0);
}