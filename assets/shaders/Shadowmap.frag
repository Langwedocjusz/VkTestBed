#version 450

layout(location = 0) in vec2 texCoord;

layout(set = 1, binding = 0) uniform sampler2D albedo_map;

layout(push_constant) uniform constants {
    mat4 Transform;
    int DoubleSided;
    float AlphaCutoff;
} PushConstants;

void main()
{
    //Sample albedo:
    vec4 albedo = texture(albedo_map, texCoord);

    //Discard fragment if transparent:
    if (albedo.a < PushConstants.AlphaCutoff)
        discard;
}