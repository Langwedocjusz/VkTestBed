#version 450

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;

layout(location = 0) out vec2 texCoord;
layout(location = 1) out vec3 normal;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 CameraViewProjection;
    mat4 LightViewProjection;
} Ubo;

layout(push_constant) uniform constants {
    vec4 AlphaCutoff;
    mat4 Transform;
} PushConstants;

// Using adjugate transform matrix to transform normal vectors:
// Credit to Inigo Quilez: https://www.shadertoy.com/view/3s33zj
mat3 adjugate(mat4 m)
{
    return mat3(cross(m[1].xyz, m[2].xyz), 
                cross(m[2].xyz, m[0].xyz), 
                cross(m[0].xyz, m[1].xyz));
}

void main() {
    mat4 MVP = Ubo.CameraViewProjection * PushConstants.Transform;

    texCoord = aTexCoord;
    normal = normalize(adjugate(PushConstants.Transform) * aNormal);

    gl_Position = MVP * vec4(aPosition, 1.0);
}