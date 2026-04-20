#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_demote_to_helper_invocation : require

#define USE_NORMAL_MAPPING
#define SOFT_SHADOWS
#define TRANSLUCENCY

//#define SSAO_DEBUG_VIEW
//#define SHADOW_DEBUG_VIEW
//#define SHADOW_DEBUG_DEPTH
//#define NO_ALBEDO_DEBUG_VIEW

#include "common/Pbr.glsl"
#include "common/SphericalHarmonics.glsl"
#include "common/DebugGrid.glsl"
#include "shadows/ShadowClient.glsl"

layout(location = 0) in VertexData {
    vec2 TexCoord;
    vec3 Normal;
    vec4 Tangent;
    vec3 FragPos;
} InData;

layout(location = 0) out vec4 outColor;

layout(scalar, set = 0, binding = 0) uniform DynamicUBOBlock {
    mat4  CameraViewProjection;
    mat4  LightViewProjection[SHADOW_NUM_CASCADES];
    float CascadeBounds[SHADOW_NUM_CASCADES];
    vec3  ViewPos;
    vec3  ViewFront;
    float DirectionalFactor;
    float EnvironmentFactor;
    float ShadowBiasLight;
    float ShadowBiasNormal;
    int   AOEnabled;
    vec2  DrawExtent; 
} DynamicUBO;

layout(scalar, set = 1, binding = 0) uniform EnvUBOBlock {
    int DirLightOn;
    vec3 LightDir;
    vec3 LightColor;
    int HdriEnabled;
    float MaxReflectionLod;
} EnvUBO;

layout(std140, set = 1, binding = 1) readonly buffer SHBuffer {
   SHData irradianceMap;
};

layout(set = 1, binding = 2) uniform samplerCube prefilteredMap;
layout(set = 1, binding = 3) uniform sampler2D integrationMap;

layout(set = 2, binding = 0) uniform sampler2DArrayShadow shadowMap;

layout(set = 3, binding = 0) uniform sampler2D aoMap;

layout(set = 4, binding = 0) uniform sampler2D albedo_map;
layout(set = 4, binding = 1) uniform sampler2D rougness_map;
layout(set = 4, binding = 2) uniform sampler2D normal_map;

layout(scalar, set = 4, binding = 3) uniform MatUBOBlock {
    float AlphaCutoff;
    vec3 TranslucentColor;
    int DoubleSided;
} MatUBO;

layout(push_constant) uniform constants {
    mat4 Model;
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

vec3 GetSkyLight(vec3 normal, vec3 view, vec3 diffuse, vec3 f0, float roughness)
{
    vec3 irradiance = SH_HemisphereConvolve(irradianceMap, normal);
    vec3 diffuseIBL = diffuse * irradiance;

    vec3 R = reflect(-view, normal);

    vec3 prefilteredColor = textureLod(prefilteredMap, R,  roughness * EnvUBO.MaxReflectionLod).rgb;

    vec3 F        = F_SchlickRoughness(max(dot(normal, view), 0.0), f0, roughness);
    vec2 envBRDF  = texture(integrationMap, vec2(max(dot(normal, view), 0.0), roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F * envBRDF.x + envBRDF.y);

    return DynamicUBO.EnvironmentFactor * (diffuseIBL + specularIBL);
}

void main()
{
    //Sample albedo:
    vec4 albedo = texture(albedo_map, InData.TexCoord);

    //Discard fragment if transparent:
    if (albedo.a < MatUBO.AlphaCutoff)
        discard;

    //Handle debug views:
    #ifdef NO_ALBEDO_DEBUG_VIEW
    albedo.rgb = vec3(1);
    #endif

    #ifdef SSAO_DEBUG_VIEW
    if (DynamicUBO.AOEnabled == 1)
    {
        vec2 aoUV = vec2(gl_FragCoord.xy) / DynamicUBO.DrawExtent;

        vec4 aoSample = texture(aoMap, aoUV);

        float ao = aoSample.a;
        outColor = vec4(vec3(ao), 1.0);
        
        //Preview reconstructed normals:
        //vec3 norm = 2.0 * aoSample.rgb - 1.0;
        //vec3 norm = aoSample.rgb;
        //outColor = vec4(norm, 1.0);
        
        return;
    }
    #endif

    #ifdef SHADOW_DEBUG_VIEW
    {
        uint cascadeIdx = GetCascadeIdx(InData.FragPos, DynamicUBO.ViewPos, DynamicUBO.ViewFront, DynamicUBO.CascadeBounds);

        vec4 lightCoord = DynamicUBO.LightViewProjection[cascadeIdx] * vec4(InData.FragPos, 1.0);
        vec3 projCoords = lightCoord.xyz / lightCoord.w;
        vec2 uv = (projCoords.xy * 0.5 + 0.5);
    
        float diff = 0.8 + 0.2 * dot(InData.Normal, normalize(vec3(1,1,1)));

        const vec3 okColors[3] = vec3[3](vec3(1.0,0.5,0.5),vec3(0.5,1.0,0.5), vec3(0.5,0.5,1.0));

        vec3 color = diff * debug_grid(uv, okColors[cascadeIdx]);

        #ifdef SHADOW_DEBUG_DEPTH
        vec3 less = vec3(1,0,0);
        vec3 more = vec3(0,0,1);
        
        color = vec3(diff);
        
        if (projCoords.z < 0.0) color *= less;
        if (projCoords.z > 1.0) color *= more;     
        #endif
    
        outColor = vec4(color, 1.0);
        return;
    }
    #endif

    //Sample roughness:
    vec4 roughnesMetallic = texture(rougness_map, InData.TexCoord);

    float roughness = roughnesMetallic.g;
    float metallic = roughnesMetallic.b;

    //Do normal mapping or use vertex normal:
    vec3 N = InData.Normal;

    //Handle two-sided geometry by flipping normals
    //facing away from view:
    if (!gl_FrontFacing)
    {
        N = -N;
    }

    #ifdef USE_NORMAL_MAPPING
    vec3 T = InData.Tangent.xyz;
    vec3 B = InData.Tangent.w * cross(N,T);

    mat3 TBN = mat3(T,B,N);

    vec3 texNormal = 2.0 * texture(normal_map, InData.TexCoord).xyz - 1.0;

    vec3 normal = normalize(TBN * texNormal);

    #else
    vec3 normal = N;
    #endif

    //Construct view vector:
    vec3 view = normalize(DynamicUBO.ViewPos - InData.FragPos);

    //Calculate specular reflectance f0:
    vec3 f0 = metallic * albedo.rgb;
    vec3 diffuse = (1.0 - metallic) * albedo.rgb;

    //Calculate lighting:
    vec4 res = vec4(0,0,0,1);

    res.rgb = GetSkyLight(normal, view, diffuse, f0, roughness);

    if (DynamicUBO.AOEnabled == 1)
    {
        vec2 aoUV = vec2(gl_FragCoord.xy) / DynamicUBO.DrawExtent;

        float ao = texture(aoMap, aoUV).a;
        
        res.rgb *= ao;
    }

    #ifdef TRANSLUCENCY
    if(MatUBO.DoubleSided == 1)
    {
        vec3 irradiance = SH_HemisphereConvolve(irradianceMap, -normal);
        res.rgb += DynamicUBO.EnvironmentFactor * MatUBO.TranslucentColor * diffuse * irradiance;
    }
    #endif

    if(EnvUBO.DirLightOn != 0)
    {
        const vec3 lcol = DynamicUBO.DirectionalFactor * EnvUBO.LightColor;

        vec3 dirResponse = BRDF(normal, view, EnvUBO.LightDir, roughness, diffuse, f0);
        
        float shadow = 1.0;
        if (dirResponse != vec3(0) || (MatUBO.DoubleSided == 1))
        {
            vec3 fragPos = InData.FragPos;

            uint cascadeIdx = GetCascadeIdx(fragPos, DynamicUBO.ViewPos, DynamicUBO.ViewFront, DynamicUBO.CascadeBounds);

            ShadowBias bias = CalculateShadowBias(N, EnvUBO.LightDir, DynamicUBO.ShadowBiasLight, DynamicUBO.ShadowBiasNormal);

            //Apply normal bias to shadowed position:
            fragPos += bias.Normal * N;

            mat4 lightProj = DynamicUBO.LightViewProjection[cascadeIdx];
            vec4 lightPos =  lightProj * vec4(fragPos, 1.0);

            #ifdef SOFT_SHADOWS
            shadow = FilterPCF(shadowMap, lightPos, bias.Light, cascadeIdx);
            #else
            shadow = CalculateShadowFactor(shadowMap, lightPos, vec2(0), bias.Light, cascadeIdx);
            #endif

            shadow = pow(shadow, 2.2);
        }

        res.rgb += shadow * lcol * dirResponse;

        #ifdef TRANSLUCENCY
        if(MatUBO.DoubleSided == 1)
        {
            vec3 translucent = BRDF(-normal, view, EnvUBO.LightDir, roughness, diffuse, f0);

            res.rgb += MatUBO.TranslucentColor * shadow * lcol * translucent;
        }
        #endif
    }

    //Do color correction:
    res.rgb = ACESFilm(res.rgb);
    res.rgb = pow(res.rgb, vec3(1.0/2.2));

    outColor = res;
}