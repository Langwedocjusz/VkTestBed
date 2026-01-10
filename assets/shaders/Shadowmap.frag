#version 450

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_demote_to_helper_invocation : require

layout(location = 0) in vec2 texCoord;

layout(set = 0, binding = 0) uniform sampler2D albedo_map;

layout(scalar, set = 0, binding = 3) uniform MatUBOBlock {
    float AlphaCutoff;
    vec3 TranslucentColor;
    int DoubleSided;
} MatUBO;

layout(push_constant) uniform constants {
    mat4 MVP;
} PushConstants;

void main()
{
    //Sample albedo:
    vec4 albedo = texture(albedo_map, texCoord);

    //Discard fragment if transparent:
    if (albedo.a < MatUBO.AlphaCutoff)
        discard;
}