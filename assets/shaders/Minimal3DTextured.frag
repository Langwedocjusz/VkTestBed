#version 450

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform constants {
    vec4 AlphaCutoff;
    mat4 Transform;
} PushConstants;

void main() {
    vec4 res = texture(texSampler, texCoord);

    if (res.a < PushConstants.AlphaCutoff.x)
        discard;

    outColor = res;
}