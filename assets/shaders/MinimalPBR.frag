#version 450

#extension GL_GOOGLE_include_directive : require

#define USE_NORMAL_MAPPING

const float PI = 3.1415926535;

#include "common/Pbr.glsl"
#include "common/SphericalHarmonics.glsl"

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec3 vertNormal;
layout(location = 2) in vec4 vertTangent;
layout(location = 3) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D albedo_map;
layout(set = 1, binding = 1) uniform sampler2D rougness_map;
layout(set = 1, binding = 2) uniform sampler2D normal_map;

layout(set = 2, binding = 0) uniform UniformBufferObject {
    vec4 LightDir;
    int HdriEnabled;
} EnvUBO;

layout(std140, set = 2, binding = 1) readonly buffer SHBuffer {
   SHData irradianceMap;
};

layout(set = 2, binding = 2) uniform samplerCube prefilteredMap;
layout(set = 2, binding = 3) uniform sampler2D integrationMap;

layout(push_constant) uniform constants {
    float AlphaCutoff;
    float PosX;
    float PosY;
    float PosZ;
    mat4 Transform;
} PushConstants;

//https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACESFilm(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;

    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

void main()
{
    //Sample albedo:
    vec4 albedo = texture(albedo_map, texCoord);

    //Discard fragment if transparent:
    if (albedo.a < PushConstants.AlphaCutoff)
        discard;

    //Sample roughness:
    vec4 roughnesMetallic = texture(rougness_map, texCoord);

    float roughness = roughnesMetallic.g;
    float metallic = roughnesMetallic.b;

    //Do normal mapping or use vertex normal:
    #ifdef USE_NORMAL_MAPPING
    vec3 texNormal = 2.0 * texture(normal_map, texCoord).xyz - 1.0;

    vec3 N = vertNormal;
    vec3 T = vertTangent.xyz;
    vec3 B = vertTangent.w * cross(N,T);

    mat3 TBN = mat3(T,B,N);

    vec3 normal = normalize(TBN * texNormal);

    #else
    vec3 normal = vertNormal;
    #endif

    //Construct view vector:
    vec3 view = vec3(PushConstants.PosX, PushConstants.PosY, PushConstants.PosZ) - fragPos;
    view = normalize(view);

    //Calculate specular reflectance f0:
    vec3 f0 = metallic * albedo.rgb;
    vec3 diffuse = (1.0 - metallic) * albedo.rgb;

    //Read in light direction:
    bool directional = (EnvUBO.LightDir.w != 0.0);
    vec3 ldir = EnvUBO.LightDir.xyz;

    //Calculate lighting:
    vec4 res = vec4(0,0,0,1);

    //Specular IBL:s
    vec3 irradiance = SHReconstruction(irradianceMap, normalize(normal));
    vec3 diffuseIBL = diffuse * irradiance;

    const float MAX_REFLECTION_LOD = 4.0;
    vec3 R = reflect(-view, normal);

    vec3 prefilteredColor = textureLod(prefilteredMap, R,  roughness * MAX_REFLECTION_LOD).rgb;

    vec3 F        = FresnelSchlickRoughness(max(dot(normal, view), 0.0), f0, roughness);
    vec2 envBRDF  = texture(integrationMap, vec2(max(dot(normal, view), 0.0), roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F * envBRDF.x + envBRDF.y);

    res.rgb = 0.4 * (diffuseIBL + specularIBL);

    if(directional)
    {
        res.rgb += BRDF(normal, view, ldir, roughness, diffuse, f0);
    }

    //Do color correction:
    res.rgb = ACESFilm(res.rgb);
    res.rgb = pow(res.rgb, vec3(1.0/2.2));

    outColor = res;
}