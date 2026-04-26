#version 450

#extension GL_EXT_demote_to_helper_invocation : require

layout(location = 0) in VertexData {
    vec2 TexCoord;
    vec3 Normal;
} vInData;

layout(location = 0) out vec4 vOutColor;

layout(set = 1, binding = 0) uniform sampler2D sTexture;

layout(push_constant) uniform PushConstants {
    vec4 AlphaCutoff;
    mat4 Transform;
} uPushConstants;

void main() {
    vec4 res = texture(sTexture, vInData.TexCoord);

    if (res.a < uPushConstants.AlphaCutoff.x)
        discard;

    vOutColor = res;
}