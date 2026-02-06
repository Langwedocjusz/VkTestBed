#version 450

layout(location = 0) in vec3 aPosition;

layout(push_constant) uniform constants {
    mat4 LightMVP;
} PushConstants;

void main() {
    gl_Position = PushConstants.LightMVP * vec4(aPosition, 1.0);
}