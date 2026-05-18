#version 450

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_demote_to_helper_invocation : require

layout(location = 0) in VertexData {
    vec2 TexCoord;
} vInData;

layout(set = 1, binding = 0) uniform sampler2D sAlbedoMap;

layout(scalar, set = 1, binding = 3) uniform MaterialBlock {
    int   DoubleSided;
    int   AlphaMode;
    float AlphaCutoff;
    vec3  TranslucentColor;
} uMaterial;

layout(push_constant) uniform PushConstants {
    mat4 Model;
} uPushConstants;

// Matches enum definitions from cpp:
#define ALPHA_MODE_MASK 1
#define ALPHA_MODE_BLEND 2

void main()
{
    //Sample albedo:
    vec4 albedo = texture(sAlbedoMap, vInData.TexCoord);

    // Handle 'mask' mode by discarding fragments
    // below the alpha cutoff:
    if (uMaterial.AlphaMode == ALPHA_MODE_MASK)
    {
        if (albedo.a < uMaterial.AlphaCutoff)
            discard;
    }
}