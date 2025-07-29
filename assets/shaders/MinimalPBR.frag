#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

#define USE_NORMAL_MAPPING

const float PI = 3.1415926535;

#include "common/Pbr.glsl"
#include "common/SphericalHarmonics.glsl"

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec3 vertNormal;
layout(location = 2) in vec4 vertTangent;
layout(location = 3) in vec3 fragPos;
layout(location = 4) in vec4 lightSpaceFragPos;

layout(location = 0) out vec4 outColor;

layout(scalar, set = 0, binding = 0) uniform DynamicUBOBlock {
    mat4 CameraViewProjection;
    mat4 LightViewProjection;
    vec3 ViewPos;
    float DirectionalFactor;
    float EnvironmentFactor;
    float ShadowBiasMin;
    float ShadowBiasMax;
} DynamicUBO;

layout(set = 1, binding = 0) uniform sampler2D albedo_map;
layout(set = 1, binding = 1) uniform sampler2D rougness_map;
layout(set = 1, binding = 2) uniform sampler2D normal_map;

layout(scalar, set = 2, binding = 0) uniform EnvUBOBlock {
    int DirLightOn;
    vec3 LightDir;
    vec3 LightColor;
    int HdriEnabled;
    float MaxReflectionLod;
} EnvUBO;

layout(std140, set = 2, binding = 1) readonly buffer SHBuffer {
   SHData irradianceMap;
};

layout(set = 2, binding = 2) uniform samplerCube prefilteredMap;
layout(set = 2, binding = 3) uniform sampler2D integrationMap;

layout(set = 3, binding = 0) uniform sampler2D shadowMap;

layout(push_constant) uniform PushConstantsBlock {
    mat4 Transform;
    int DoubleSided;
    float AlphaCutoff;
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

float CalculateShadowFactor(vec4 lightCoord, vec2 texOffset)
{
    vec3 projCoords = lightCoord.xyz / lightCoord.w;
    vec2 uv = projCoords.xy * 0.5 + 0.5;

    //if (projCoords.z > 1.0)
    //    return 1.0;

    float closestDepth = texture(shadowMap, uv + texOffset).r;
    float currentDepth = projCoords.z;

    float bias = max(DynamicUBO.ShadowBiasMax * (1.0 - dot(vertNormal, EnvUBO.LightDir)), DynamicUBO.ShadowBiasMin);
    float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;

    return 1.0 - shadow;
}

float FilterPCF(vec4 lightCoord)
{
    const float scale = 1.0;
	ivec2 texDim = textureSize(shadowMap, 0);
	
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadowFactor += CalculateShadowFactor(lightCoord, vec2(dx*x, dy*y));
			count++;
		}
	
	}

	return shadowFactor / count;
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
    vec3 view = normalize(DynamicUBO.ViewPos - fragPos);

    //Handle two-sided geometry by flipping normals
    //facing away from view
    if (!gl_FrontFacing)
        normal = -normal;

    //Calculate specular reflectance f0:
    vec3 f0 = metallic * albedo.rgb;
    vec3 diffuse = (1.0 - metallic) * albedo.rgb;

    //Calculate lighting:
    vec4 res = vec4(0,0,0,1);

    //Specular IBL:s
    vec3 irradiance = SH_HemisphereConvolve(irradianceMap, normal);
    vec3 diffuseIBL = diffuse * irradiance;

    vec3 R = reflect(-view, normal);

    vec3 prefilteredColor = textureLod(prefilteredMap, R,  roughness * EnvUBO.MaxReflectionLod).rgb;

    vec3 F        = F_SchlickRoughness(max(dot(normal, view), 0.0), f0, roughness);
    vec2 envBRDF  = texture(integrationMap, vec2(max(dot(normal, view), 0.0), roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F * envBRDF.x + envBRDF.y);

    res.rgb = DynamicUBO.EnvironmentFactor * (diffuseIBL + specularIBL);

    if(EnvUBO.DirLightOn != 0)
    {
        const vec3 lcol = DynamicUBO.DirectionalFactor * EnvUBO.LightColor;

        vec3 dirResponse = BRDF(normal, view, EnvUBO.LightDir, roughness, diffuse, f0);
        float shadow = FilterPCF(lightSpaceFragPos);

        res.rgb += shadow * lcol * dirResponse;
    }

    //Do color correction:
    res.rgb = ACESFilm(res.rgb);
    res.rgb = pow(res.rgb, vec3(1.0/2.2));

    outColor = res;
}