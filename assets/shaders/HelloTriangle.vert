#version 450

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;

layout(location = 0) out vec3 fragColor;

layout( push_constant ) uniform constants
{
    //temporary vec4 to avoid possible alignment issues:
    vec4 Translation;
} PushConstants;

void main() {
    vec3 offset = PushConstants.Translation.xyz;

    gl_Position = vec4(aPosition + offset, 1.0);
    fragColor = aColor;
}