#define SHADOW_NUM_CASCADES 3

uint GetCascadeIdx(vec3 fragPos, vec3 viewPos, vec3 viewFront, float cascadeBounds[SHADOW_NUM_CASCADES])
{
    uint cascadeIdx = 0;

    for (int i=0; i<SHADOW_NUM_CASCADES; i++)
    {
        // Get normalized view vector and distance to fragment:
        vec3 viewDir = fragPos - viewPos;
        
        float dist = length(viewDir);
        viewDir = normalize(viewDir);

        // Project the distance to view axis:
        float projDist = dist * dot(viewDir, viewFront);
        
        //Compare with stored bounds:
        float maxDist = cascadeBounds[i];

        if (projDist < maxDist)
        {
            cascadeIdx = i;
            break;
        }   
    }

    return cascadeIdx;
}

struct ShadowBias{
    float Light;
    float Normal;
};

// TODO: Adjust biases based on distance/cascade. Scaling with per-cascade
// world-space texel size seems reasonable.
ShadowBias CalculateShadowBias(vec3 normal, vec3 lightDir, float maxBiasLight, float maxBiasNormal)
{
    float cosAlpha = dot(normal, lightDir);
    float sinAlpha = sqrt(1 - cosAlpha*cosAlpha); 

    float normalBias = maxBiasNormal * sinAlpha;

    // According to Ignacio Castano's blog 
    // (https://www.ludicon.com/castano/blog/articles/shadow-mapping-summary-part-1/)
    // light bias should be scaled by the tangent, like so:
    
    // float safeTan = min(2.0, sinAlpha / cosAlpha);
    // float lightBias = maxBiasLight * safeTan;
    
    // While this tends to work better on opaque surfaces,
    // very problematic for double-sided
    // translucent foliage, where it introduced
    // erroneous self-shadowing. For now scaling
    // with just the sine. This also hase the positive 
    // effect of making shadow offsets smaller.

    float lightBias = maxBiasLight * sinAlpha;

    return ShadowBias(lightBias, normalBias);
}

float CalculateShadowFactor(sampler2DArrayShadow shadowMap, vec4 lightCoord, vec2 offset, float bias, uint cascadeIdx)
{
    vec3 projCoords = lightCoord.xyz / lightCoord.w;
    vec2 uv = (projCoords.xy * 0.5 + 0.5) + offset;

    if (projCoords.z > 1.0)
        return 1.0;    

    float currentDepth = projCoords.z - bias;

    float shadow = texture(shadowMap, vec4(uv, cascadeIdx, currentDepth));

    return shadow;
}

// TODO: Implement filtering optimization by Ignacio Castano.
float FilterPCF(sampler2DArrayShadow shadowMap, vec4 lightCoord, float bias, uint cascadeIdx)
{
    const float range = 1.5;
    const vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    // Crude way to maintain consistend level of blurring
    // between cascades:
    const vec2 scale = texelSize / float(cascadeIdx + 1);

    float sum = 0.0, norm = 0.0;

    for (float y = -range; y <= range; y += 1.0)
    {
        for (float x = -range; x <= range; x += 1.0)
        {
            vec2 offset = scale * vec2(x,y);
            
            sum += CalculateShadowFactor(shadowMap, lightCoord, offset, bias, cascadeIdx);
            norm += 1.0;
        }
    }

    return sum / norm;
}
