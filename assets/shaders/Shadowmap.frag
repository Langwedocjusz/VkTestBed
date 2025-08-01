#version 450

layout(location = 0) in vec2 texCoord;

layout(set = 1, binding = 0) uniform sampler2D albedo_map;

layout(push_constant) uniform constants {
    mat4 Transform;
    vec4 TransAlpha;
    int DoubleSided;
} PushConstants;

void main()
{
    //Sample albedo:
    vec4 albedo = texture(albedo_map, texCoord);

    //Discard fragment if transparent:
    float alphaCutoff = PushConstants.TransAlpha.a;

    if (albedo.a < alphaCutoff)
        discard;
}