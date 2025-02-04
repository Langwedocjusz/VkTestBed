#version 450

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aTangent;

layout(location = 0) out vec2 texCoord;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec4 tangent;

layout(location = 3) out vec3 fragPos;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 ViewProjection;
} Ubo;

layout(push_constant) uniform constants {
    vec4 OtherThings;
    mat4 Transform;
} PushConstants;

void main() {
    mat4 MVP = Ubo.ViewProjection * PushConstants.Transform;

    mat3 NORMAL = mat3(transpose(inverse(PushConstants.Transform)));

    texCoord = aTexCoord;
    normal = normalize(NORMAL * aNormal);
    tangent = aTangent;

    if (tangent.xyz != vec3(0))
        tangent.xyz = normalize(NORMAL * tangent.xyz);

    fragPos = vec3(PushConstants.Transform * vec4(aPosition, 1.0));

    gl_Position = MVP * vec4(aPosition, 1.0);
}