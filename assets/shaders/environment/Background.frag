#version 450

layout(location = 0) in VertexData {
    vec2 TexCoord;
} vIn;

layout(location = 0) out vec4 vOutColor;

layout(set = 0, binding = 0) uniform samplerCube sEnvironment;

layout(push_constant) uniform PushConstants {
    vec4 TopLeft;
    vec4 TopRight;
    vec4 BottomLeft;
    vec4 BottomRight;
} uPushConstants;

void main()
{
    //Retrieve world-space direction:
    float x = vIn.TexCoord.x;
    float y = vIn.TexCoord.y;

    vec3 top = mix(uPushConstants.TopLeft.xyz,    uPushConstants.TopRight.xyz,    x);
    vec3 bot = mix(uPushConstants.BottomLeft.xyz, uPushConstants.BottomRight.xyz, x);

    vec3 dir = mix(top, bot, y);

    dir = normalize(dir);

    //Sample environment:
    vec3 col = texture(sEnvironment, dir).rgb;

    //Output:
    vOutColor = vec4(col, 1.0);
}