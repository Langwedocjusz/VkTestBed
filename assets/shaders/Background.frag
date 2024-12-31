#version 450

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D environment;

layout(push_constant) uniform constants {
    vec4 TopLeft;
    vec4 TopRight;
    vec4 BottomLeft;
    vec4 BottomRight;
} PushConstants;

void main()
{
    //Retrieve world-space direction
    vec3 dir = mix(
        mix(PushConstants.TopLeft.xyz, PushConstants.TopRight.xyz, uv.x),
        mix(PushConstants.BottomLeft.xyz, PushConstants.BottomRight.xyz, uv.x),
    uv.y);

    dir = normalize(dir);

    //Recover equirectangular tex coords:
    vec2 texCoord;
    const float pi = 3.1415926535;

    texCoord.x = atan(dir.x, dir.z) / (2.0 * pi);
    texCoord.y = (asin(dir.y) / pi) + 0.5;

    //Sample environment:
    vec3 col = texture(environment, texCoord).rgb;

    //Output:
    outColor = vec4(col, 1.0);
}