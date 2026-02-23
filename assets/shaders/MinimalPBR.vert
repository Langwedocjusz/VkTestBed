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
} OutData;

layout(scalar, set = 0, binding = 0) uniform DynamicUBOBlock {
    mat4 CameraViewProjection;
    mat4 LightViewProjection[3]; //TODO: Must be kept in-sync with shadowmap cascades
    float CascadeBounds[3];
    vec3 ViewPos;
    vec3 ViewFront;
    float DirectionalFactor;
    float EnvironmentFactor;
    float ShadowBiasMin;
    float ShadowBiasMax;
} Ubo;

layout(push_constant) uniform constants {
    mat4 Model;
} PushConstants;

// Using adjugate transform matrix to transform normal vectors:
// Credit to Inigo Quilez: https://www.shadertoy.com/view/3s33zj
mat3 adjugate(mat4 m)
{
    return mat3(cross(m[1].xyz, m[2].xyz), 
                cross(m[2].xyz, m[0].xyz), 
                cross(m[0].xyz, m[1].xyz));
}

void main() {
    mat4 MVP = Ubo.CameraViewProjection * PushConstants.Model;

    vec3 normal = normalize(adjugate(PushConstants.Model) * aNormal);
    
    vec3 tangent = vec3(PushConstants.Model * vec4(aTangent.xyz, 0.0));
    float bitangentSign = aTangent.w;

    gl_Position = MVP * vec4(aPosition, 1.0);

    OutData.TexCoord = aTexCoord;
    OutData.Normal = normal;
    OutData.Tangent = vec4(tangent, bitangentSign);

    OutData.FragPos = vec3(PushConstants.Model * vec4(aPosition, 1.0));
    OutData.FragDistance = length(OutData.FragPos - Ubo.ViewPos);
}