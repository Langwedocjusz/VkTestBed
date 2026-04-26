#version 450

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_demote_to_helper_invocation : require

layout(location = 0) in VertexData {
    vec2 TexCoord;
} vInData;

layout(set = 0, binding = 0) uniform sampler2D sAlbedoMap;

layout(scalar, set = 0, binding = 3) uniform MaterialBlock {
    float AlphaCutoff;
    vec3  TranslucentColor;
    int   DoubleSided;
} uMaterial;

layout(push_constant) uniform constants {
    mat4 MVP;
} PushConstants;

void main()
{
    //Sample albedo:
    vec4 albedo = texture(sAlbedoMap, vInData.TexCoord);

    //Discard fragment if transparent:
    if (albedo.a < uMaterial.AlphaCutoff)
        discard;
}