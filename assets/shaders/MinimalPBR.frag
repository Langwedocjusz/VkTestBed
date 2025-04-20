#version 450

#define USE_NORMAL_MAPPING

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

struct SHData {
    vec4 SH_L0;
    vec4 SH_L1_M_1;
    vec4 SH_L1_M0;
    vec4 SH_L1_M1;
    vec4 SH_L2_M_2;
    vec4 SH_L2_M_1;
    vec4 SH_L2_M0;
    vec4 SH_L2_M1;
    vec4 SH_L2_M2;
};

layout(std140, set = 2, binding = 1) readonly buffer SHBuffer {
   SHData irradianceMap;
};

layout(push_constant) uniform constants {
    float AlphaCutoff;
    float PosX;
    float PosY;
    float PosZ;
    mat4 Transform;
} PushConstants;

const float PI = 3.1415926535;

//PBR functions from: https://google.github.io/filament/Filament.md.html

float D_GGX(float NoH, float roughness)
{
    float a = NoH * roughness;
    float k = roughness / (1.0 - NoH * NoH + a * a);
    return k * k * (1.0 / PI);
}

float V_SmithGGXCorrelatedFast(float NoV, float NoL, float roughness)
{
    float a = roughness;
    float GGXV = NoL * (NoV * (1.0 - a) + a);
    float GGXL = NoV * (NoL * (1.0 - a) + a);
    return 0.5 / (GGXV + GGXL);
}

vec3 F_Schlick(float u, vec3 f0)
{
    float f = pow(1.0 - u, 5.0);
    return f + f0 * (1.0 - f);
}

float Fd_Lambert()
{
    return 1.0 / PI;
}

vec3 BRDF(vec3 n, vec3 v, vec3 l, float perceptualRoughness, vec3 diffuseColor, vec3 f0)
{
    vec3 h = normalize(v + l);

    float NoV = abs(dot(n, v)) + 1e-5;
    float NoL = clamp(dot(n, l), 0.0, 1.0);
    float NoH = clamp(dot(n, h), 0.0, 1.0);
    float LoH = clamp(dot(l, h), 0.0, 1.0);

    // perceptually linear roughness to roughness
    float roughness = perceptualRoughness * perceptualRoughness;

    float D = D_GGX(NoH, roughness);
    vec3  F = F_Schlick(LoH, f0);
    float V = V_SmithGGXCorrelatedFast(NoV, NoL, roughness);

    // specular BRDF
    vec3 Fr = (D * V) * F;
    Fr = max(vec3(0.0), Fr);

    // diffuse BRDF
    vec3 Fd = diffuseColor * Fd_Lambert();

    //return max(vec3(0.0), (Fr + Fd) * NoL);
    return (Fr + Fd) * NoL;
}

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

vec3 SHReconstruction(SHData data, vec3 dir)
{
    float x = dir.x, y = dir.y, z = dir.z;

    float SH_L0     = 0.28209479177;
    float SH_L1_M_1 = 0.48860251190 * y;
    float SH_L1_M0  = 0.48860251190 * z;
    float SH_L1_M1  = 0.48860251190 * x;
    float SH_L2_M_2 = 1.09254843059 * x*y;
    float SH_L2_M_1 = 1.09254843059 * y*z;
    float SH_L2_M0  = 0.31539156525 * (3.0*z*z - 1.0);
    float SH_L2_M1  = 1.09254843059 * x*z;
    float SH_L2_M2  = 0.54627421529 * (x*x - y*y);

    vec3 res = vec3(0);

    res += data.SH_L0.rgb * SH_L0;
    res += data.SH_L1_M_1.rgb * SH_L1_M_1;
    res += data.SH_L1_M0.rgb * SH_L1_M0;
    res += data.SH_L1_M1.rgb * SH_L1_M1;
    res += data.SH_L2_M_2.rgb * SH_L2_M_2;
    res += data.SH_L2_M_1.rgb * SH_L2_M_1;
    res += data.SH_L2_M0.rgb * SH_L2_M0;
    res += data.SH_L2_M1.rgb * SH_L2_M1;
    res += data.SH_L2_M2.rgb * SH_L2_M2;

    return res;
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

    if(directional)
    {
        res.rgb += BRDF(normal, view, ldir, roughness, diffuse, f0);

        //Backlighting:
        const vec3 skyCol = vec3(0.0, 0.05, 0.1);
        res.rgb += skyCol * BRDF(normal, view, -ldir, roughness, diffuse, f0);
    }

    //Do color correction:
    res.rgb = ACESFilm(res.rgb);
    res.rgb = pow(res.rgb, vec3(1.0/2.2));

    //res.rgb = SHReconstruction(irradianceMap, normalize(normal));
    //res.rgb = albedo.rgb * SHReconstruction(irradianceMap, normalize(normal));

    outColor = res;
}