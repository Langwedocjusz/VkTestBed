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

float G_SmithGGXCorrelated(float NoV, float NoL, float roughness)
{
    return 4.0 * NoV * NoL * V_SmithGGXCorrelatedFast(NoV, NoL, roughness);
}

vec3 F_Schlick(float u, vec3 f0)
{
    float f = pow(1.0 - u, 5.0);
    return f + f0 * (1.0 - f);
}

vec3 F_SchlickRoughness(float u, vec3 f0, float roughness)
{
    float f =  pow(1.0 - u, 5.0);
    vec3 A = max(vec3(1.0 - roughness), f0);

    return A*f + f0 * (1.0 - f);
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

    // diffuse BRDF
    vec3 Fd = diffuseColor * Fd_Lambert();

    // specular BRDF
    vec3 Fr = (D * V) * F;
    Fr = max(vec3(0.0), Fr);

    //return max(vec3(0.0), (Fr + Fd) * NoL);
    return (Fr + Fd) * NoL;
}