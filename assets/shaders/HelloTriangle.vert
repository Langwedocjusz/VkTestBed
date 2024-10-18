#version 450

layout(location = 0) out vec3 fragColor;

const float r3 = sqrt(3.0);

vec3 positions[3] = vec3[](
    vec3( 0.0,-r3/3.0, 0.0),
    vec3( 0.5, r3/6.0, 0.0),
    vec3(-0.5, r3/6.0, 0.0)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 1.0);
    fragColor = colors[gl_VertexIndex];
}