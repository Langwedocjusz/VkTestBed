#version 450

layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vColor;
layout(location = 2) in vec3 vNormal;

layout(location = 0) out VertexData{
    vec3 Color;
} vOut;

layout(binding = 0) uniform DynamicBlock {
    mat4 CameraViewProjection;
    mat4 LightViewProjection;
} uDynamic;

layout(push_constant) uniform PushConstants {
    mat4 Transform;
} uPushConstants;

void main() {
    mat4 MVP = uDynamic.CameraViewProjection * uPushConstants.Transform;

    vOut.Color = vColor;
    gl_Position = MVP * vec4(vPosition, 1.0);
}