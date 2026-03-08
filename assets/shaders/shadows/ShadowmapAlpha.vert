#version 450

#extension GL_GOOGLE_include_directive : require

#include "../common/VertexNaive.glsl"
//#include "../common/VertexCompressed.glsl"

layout(location = 0) out vec2 texCoord;

layout(push_constant) uniform constants {
    mat4 LightMVP;
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
    
    gl_Position = PushConstants.LightMVP * vec4(position, 1.0);

    texCoord = texcoord;
}