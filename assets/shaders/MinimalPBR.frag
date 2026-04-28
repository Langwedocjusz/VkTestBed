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
} vInData;

layout(location = 0) out vec4 vOutColor;

layout(scalar, set = 0, binding = 0) uniform CameraBlock {
    mat4 ViewProjection;
    vec3 Pos;
    vec3 Front;
} uCamera;

layout(scalar, set = 0, binding = 1) uniform DynamicBlock {
    mat4  LightViewProjection[SHADOW_NUM_CASCADES];
    float CascadeBounds[SHADOW_NUM_CASCADES];
    float CascadeTexelSizes[SHADOW_NUM_CASCADES];
    float DirectionalFactor;
    float EnvironmentFactor;
    float ShadowBiasLight;
    float ShadowBiasNormal;
    int   AOEnabled;
    vec2  DrawExtent; 
} uDynamic;

layout(scalar, set = 1, binding = 0) uniform EnvBlock {
    int DirLightOn;
    vec3 LightDir;
    vec3 LightColor;
    int HdriEnabled;
    float MaxReflectionLod;
} uEnv;

layout(std140, set = 1, binding = 1) readonly buffer SHBuffer {
   SHData bIrradiance;
};

layout(set = 1, binding = 2) uniform samplerCube sPrefilteredMap;
layout(set = 1, binding = 3) uniform sampler2D sIntegrationMap;

layout(set = 2, binding = 0) uniform sampler2DArrayShadow sShadowMap;
layout(set = 2, binding = 1) uniform sampler2D sAOMap;

layout(set = 3, binding = 0) uniform sampler2D sAlbedoMap;
layout(set = 3, binding = 1) uniform sampler2D sRougnessMap;
layout(set = 3, binding = 2) uniform sampler2D sNormalMap;

layout(scalar, set = 3, binding = 3) uniform MaterialBlock {
    float AlphaCutoff;
    vec3  TranslucentColor;
    int   DoubleSided;
} uMaterial;

layout(push_constant) uniform PushConstants {
    mat4 Model;
} uPushConstants;

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
    vec3 irradiance = SH_HemisphereConvolve(bIrradiance, normal);
    vec3 diffuseIBL = diffuse * irradiance;

    vec3 R = reflect(-view, normal);

    vec3 prefilteredColor = textureLod(sPrefilteredMap, R,  roughness * uEnv.MaxReflectionLod).rgb;

    vec3 F        = F_SchlickRoughness(max(dot(normal, view), 0.0), f0, roughness);
    vec2 envBRDF  = texture(sIntegrationMap, vec2(max(dot(normal, view), 0.0), roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F * envBRDF.x + envBRDF.y);

    return uDynamic.EnvironmentFactor * (diffuseIBL + specularIBL);
}

void main()
{
    //Sample albedo:
    vec4 albedo = texture(sAlbedoMap, vInData.TexCoord);

    //Discard fragment if transparent:
    if (albedo.a < uMaterial.AlphaCutoff)
        discard;

    //Handle debug views:
    #ifdef NO_ALBEDO_DEBUG_VIEW
    albedo.rgb = vec3(1);
    #endif

    #ifdef SSAO_DEBUG_VIEW
    if (uDynamic.AOEnabled == 1)
    {
        vec2 aoUV = vec2(gl_FragCoord.xy) / uDynamic.DrawExtent;

        vec4 aoSample = texture(sAOMap, aoUV);

        float ao = aoSample.a;
        vOutColor = vec4(vec3(ao), 1.0);
        
        //Preview reconstructed normals:
        //vec3 norm = 2.0 * aoSample.rgb - 1.0;
        //vec3 norm = aoSample.rgb;
        //vOutColor = vec4(norm, 1.0);
        
        return;
    }
    #endif

    #ifdef SHADOW_DEBUG_VIEW
    {
        uint cascadeIdx = GetCascadeIdx(vInData.FragPos, uCamera.Pos, uCamera.Front, uDynamic.CascadeBounds);

        vec4 lightCoord = uDynamic.LightViewProjection[cascadeIdx] * vec4(vInData.FragPos, 1.0);
        vec3 projCoords = lightCoord.xyz / lightCoord.w;
        vec2 uv = (projCoords.xy * 0.5 + 0.5);
    
        float diff = 0.8 + 0.2 * dot(vInData.Normal, normalize(vec3(1,1,1)));

        const vec3 okColors[3] = vec3[3](vec3(1.0,0.5,0.5),vec3(0.5,1.0,0.5), vec3(0.5,0.5,1.0));

        vec3 color = diff * DebugGrid(uv, okColors[cascadeIdx]);

        #ifdef SHADOW_DEBUG_DEPTH
        vec3 less = vec3(1,0,0);
        vec3 more = vec3(0,0,1);
        
        color = vec3(diff);
        
        if (projCoords.z < 0.0) color *= less;
        if (projCoords.z > 1.0) color *= more;     
        #endif
    
        vOutColor = vec4(color, 1.0);
        return;
    }
    #endif

    //Sample roughness:
    vec4 roughnesMetallic = texture(sRougnessMap, vInData.TexCoord);

    float roughness = roughnesMetallic.g;
    float metallic = roughnesMetallic.b;

    //Do normal mapping or use vertex normal:
    vec3 N = vInData.Normal;

    //Handle two-sided geometry by flipping normals
    //facing away from view:
    if (!gl_FrontFacing)
    {
        N = -N;
    }

    #ifdef USE_NORMAL_MAPPING
    vec3 T = vInData.Tangent.xyz;
    vec3 B = vInData.Tangent.w * cross(N,T);

    mat3 TBN = mat3(T,B,N);

    vec3 texNormal = 2.0 * texture(sNormalMap, vInData.TexCoord).xyz - 1.0;

    vec3 normal = normalize(TBN * texNormal);

    #else
    vec3 normal = N;
    #endif

    //Construct view vector:
    vec3 view = normalize(uCamera.Pos - vInData.FragPos);

    //Calculate specular reflectance f0:
    vec3 f0 = metallic * albedo.rgb;
    vec3 diffuse = (1.0 - metallic) * albedo.rgb;

    //Calculate lighting:
    vec4 res = vec4(0,0,0,1);

    res.rgb = GetSkyLight(normal, view, diffuse, f0, roughness);

    if (uDynamic.AOEnabled == 1)
    {
        vec2 aoUV = vec2(gl_FragCoord.xy) / uDynamic.DrawExtent;

        float ao = texture(sAOMap, aoUV).a;
        
        res.rgb *= ao;
    }

    #ifdef TRANSLUCENCY
    if(uMaterial.DoubleSided == 1)
    {
        vec3 irradiance = SH_HemisphereConvolve(bIrradiance, -normal);
        res.rgb += uDynamic.EnvironmentFactor * uMaterial.TranslucentColor * diffuse * irradiance;
    }
    #endif

    if(uEnv.DirLightOn != 0)
    {
        const vec3 lcol = uDynamic.DirectionalFactor * uEnv.LightColor;

        vec3 dirResponse = BRDF(normal, view, uEnv.LightDir, roughness, diffuse, f0);
        
        float shadow = 1.0;
        if (dirResponse != vec3(0) || (uMaterial.DoubleSided == 1))
        {
            vec3 fragPos = vInData.FragPos;

            uint cascadeIdx = GetCascadeIdx(fragPos, uCamera.Pos, uCamera.Front, uDynamic.CascadeBounds);

            float maxBiasL = uDynamic.ShadowBiasLight  * uDynamic.CascadeTexelSizes[0];
            float maxBiasN = uDynamic.ShadowBiasNormal * uDynamic.CascadeTexelSizes[0];

            ShadowBias bias = CalculateShadowBias(N, uEnv.LightDir, maxBiasL, maxBiasN);

            //Apply normal bias to shadowed position:
            fragPos += bias.Normal * N;

            mat4 lightProj = uDynamic.LightViewProjection[cascadeIdx];
            vec4 lightPos =  lightProj * vec4(fragPos, 1.0);

            #ifdef SOFT_SHADOWS
            shadow = FilterPCF(sShadowMap, lightPos, bias.Light, cascadeIdx);
            #else
            shadow = CalculateShadowFactor(sShadowMap, lightPos, vec2(0), bias.Light, cascadeIdx);
            #endif

            shadow = pow(shadow, 2.2);
        }

        res.rgb += shadow * lcol * dirResponse;

        #ifdef TRANSLUCENCY
        if(uMaterial.DoubleSided == 1)
        {
            vec3 translucent = BRDF(-normal, view, uEnv.LightDir, roughness, diffuse, f0);

            res.rgb += uMaterial.TranslucentColor * shadow * lcol * translucent;
        }
        #endif
    }

    //Do color correction:
    res.rgb = ACESFilm(res.rgb);
    res.rgb = pow(res.rgb, vec3(1.0/2.2));

    vOutColor = res;
}