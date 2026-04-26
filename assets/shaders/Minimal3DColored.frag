#version 450

layout(location = 0) in VertexData{
    vec3 Color;
} vInData;

layout(location = 0) out vec4 vOutColor;

void main() {
    vOutColor = vec4(vInData.Color, 1.0);
}