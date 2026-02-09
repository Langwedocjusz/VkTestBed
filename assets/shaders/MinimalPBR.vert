#version 450

#extension GL_EXT_scalar_block_layout : require

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aTangent;

layout(location = 0) out VertexData {
    vec2 TexCoord;
    vec3 Normal;
    vec4 Tangent;
    vec3 FragPos;
    float FragDistance;
    //vec4 LightSpaceFragPos;
} OutData;

layout(scalar, set = 0, binding = 0) uniform DynamicUBOBlock {
    mat4 CameraViewProjection;
    mat4 LightViewProjection[3]; //TODO: Must be kept in-sync with shadowmap cascades
    float CascadeBounds[3];
    vec3 ViewPos;
    float DirectionalFactor;
    float EnvironmentFactor;
    float ShadowBiasMin;
    float ShadowBiasMax;
} Ubo;

layout(push_constant) uniform constants {
    mat4 Model;
    mat4 Normal;
} PushConstants;

void main() {
    mat4 MVP = Ubo.CameraViewProjection * PushConstants.Model;

    gl_Position = MVP * vec4(aPosition, 1.0);

    OutData.TexCoord = aTexCoord;
    OutData.Normal = normalize(mat3(PushConstants.Normal) * aNormal);
    OutData.Tangent = aTangent;

    if (aTangent.xyz != vec3(0))
        OutData.Tangent.xyz = normalize(mat3(PushConstants.Normal) * aTangent.xyz);

    OutData.FragPos = vec3(PushConstants.Model * vec4(aPosition, 1.0));

    OutData.FragDistance = length(OutData.FragPos - Ubo.ViewPos);

    //OutData.LightSpaceFragPos = Ubo.LightViewProjection[0] * vec4(OutData.FragPos, 1.0);
}