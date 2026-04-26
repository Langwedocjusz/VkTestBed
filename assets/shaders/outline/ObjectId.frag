#version 450

#extension GL_EXT_scalar_block_layout : require

layout(location = 0) in VertexData {
    vec2 TexCoord;
} vInData;

//layout(location = 0) out uvec4 outColor;
layout(location = 0) out uvec4 vOutColor;

layout(set = 1, binding = 0) uniform sampler2D sAlbedoMap;

layout(scalar, set = 1, binding = 3) uniform MaterialBlock {
    float AlphaCutoff;
    vec3  TranslucentColor;
    int   DoubleSided;
} uMaterial;

layout(push_constant) uniform PushConstants {
    mat4 MVP;
    // For proper alignment of ObjectId without declaring
    // buffer extension etc in the fragment shader:
    uint DeviceAddress1;
    uint DeviceAddress2;
    vec2 TexCenter;
    vec2 TexExtent;
    uint ObjectId;
} uPushConstants;

void main()
{
    //Sample albedo:
    vec4 albedo = texture(sAlbedoMap, vInData.TexCoord);

    //Discard fragment if transparent:
    if (albedo.a < uMaterial.AlphaCutoff)
        discard;

    vOutColor = uvec4(
        (uPushConstants.ObjectId >> 0)  & 255,
        (uPushConstants.ObjectId >> 8)  & 255,
        (uPushConstants.ObjectId >> 16) & 255,
        (uPushConstants.ObjectId >> 24) & 255
    );
}