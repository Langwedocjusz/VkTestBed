#version 450

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;

layout(location = 0) out vec2 texCoord;

layout(push_constant) uniform constants {
    mat4 LightMVP;
} PushConstants;

void main() {
    texCoord = aTexCoord;
    gl_Position = PushConstants.LightMVP * vec4(aPosition, 1.0);
}