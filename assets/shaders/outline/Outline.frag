#version 450

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_demote_to_helper_invocation : require

layout(location = 0) in VertexData {
    vec2 TexCoord;
} vInData;

layout(location = 0) out vec4 vOutColor;

layout(set = 1, binding = 0) uniform sampler2D sAlbedoMap;

layout(scalar, set = 1, binding = 3) uniform MaterialBlock {
    float AlphaCutoff;
    vec3  TranslucentColor;
    int   DoubleSided;
} uMaterial;

layout(push_constant) uniform constants {
    mat4 Model;
} PushConstants;

void main()
{
    //Sample albedo:
    vec4 albedo = texture(sAlbedoMap, vInData.TexCoord);

    //Discard fragment if transparent:
    if (albedo.a < uMaterial.AlphaCutoff)
        discard;

    //Draw the outline:
    const vec4 outlineColor = vec4(1.0);
    vOutColor = outlineColor;
}