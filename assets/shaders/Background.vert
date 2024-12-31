#version 450

layout(location = 0) out vec2 uv;

void main()
{
    //Positions for a fullscreen triangle:
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2(-1.0, 3.0),
        vec2(3.0, -1.0)
    );

    //Max z-depth (requires "less or equal" depth op):
    gl_Position = vec4(positions[gl_VertexIndex], 1.0, 1.0);

    uv = positions[gl_VertexIndex];
    uv = 0.5 * uv + 0.5;
    //Flip y orientation:
    //uv.y = 1.0 - uv.y;
}