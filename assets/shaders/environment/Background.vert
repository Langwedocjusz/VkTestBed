#version 450

layout(location = 0) out VertexData {
    vec2 TexCoord;
} vOut;

void main()
{
    //Positions for a fullscreen triangle:
    const vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2(-1.0, 3.0),
        vec2(3.0, -1.0)
    );

    vec2 pos = positions[gl_VertexIndex];

    vec2 uv = 0.5 * pos + 0.5;
    //Flip y orientation:
    //uv.y = 1.0 - uv.y;

    vOut.TexCoord = uv;

    //Max z-depth (requires "less or equal" depth op):
    gl_Position = vec4(pos, 1.0, 1.0);
}