#version 450

#extension GL_EXT_scalar_block_layout : require

layout(location = 0) in VertexData {
    vec2 TexCoord;
} InData;

//layout(location = 0) out uvec4 outColor;
layout(location = 0) out uvec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D albedo_map;

layout(scalar, set = 1, binding = 3) uniform MatUBOBlock {
    float AlphaCutoff;
    vec3 TranslucentColor;
    int DoubleSided;
} MatUBO;

layout(push_constant) uniform constants {
    mat4 MVP;
    uint ObjectId;
} PushConstants;

void main()
{
    //Sample albedo:
    vec4 albedo = texture(albedo_map, InData.TexCoord);

    //Discard fragment if transparent:
    if (albedo.a < MatUBO.AlphaCutoff)
        discard;

    outColor = uvec4(
        (PushConstants.ObjectId >> 0)  & 255,
        (PushConstants.ObjectId >> 8)  & 255,
        (PushConstants.ObjectId >> 16) & 255,
        (PushConstants.ObjectId >> 24) & 255
    );
}