#version 450

layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec2 vTexCoord;
layout(location = 2) in vec3 vNormal;

layout(location = 0) out VertexData{
    vec2 TexCoord;
    vec3 Normal;
} vOut;

layout(set = 0, binding = 0) uniform DynamicBlock {
    mat4 CameraViewProjection;
    mat4 LightViewProjection;
} uDynamic;

layout(push_constant) uniform PushConstants {
    vec4 AlphaCutoff;
    mat4 Transform;
} uPushConstants;

// Using adjugate transform matrix to transform normal vectors:
// Credit to Inigo Quilez: https://www.shadertoy.com/view/3s33zj
mat3 adjugate(mat4 m)
{
    return mat3(cross(m[1].xyz, m[2].xyz), 
                cross(m[2].xyz, m[0].xyz), 
                cross(m[0].xyz, m[1].xyz));
}

void main() {
    mat4 MVP = uDynamic.CameraViewProjection * uPushConstants.Transform;

    vOut.TexCoord = vTexCoord;
    vOut.Normal = normalize(adjugate(uPushConstants.Transform) * vNormal);

    gl_Position = MVP * vec4(vPosition, 1.0);
}