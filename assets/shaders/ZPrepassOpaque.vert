#version 450

#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "common/VertexNaive.glsl"
//#include "common/VertexCompressed.glsl"

layout(scalar, set = 0, binding = 0) uniform CameraBlock {
    mat4 ViewProjection;
    vec3 Pos;
    vec3 Front;
} uCamera;

layout(push_constant) uniform PushConstants {
    mat4 Model;
    VertexBuffer VertBuff;
} uPushConstants;

void main() {
    Vertex vert = uPushConstants.VertBuff.Vertices[gl_VertexIndex];
    
    vec3 position = GetPosition(vert);

    mat4 MVP = uCamera.ViewProjection * uPushConstants.Model;
    
    gl_Position = MVP * vec4(position, 1.0);
}