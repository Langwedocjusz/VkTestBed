#version 450

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec3 vertNormal;
layout(location = 2) in vec4 vertTangent;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D albedo_map;
layout(set = 1, binding = 1) uniform sampler2D rougness_map;
layout(set = 1, binding = 2) uniform sampler2D normal_map;

layout(push_constant) uniform constants {
    vec4 AlphaCutoff;
    mat4 Transform;
} PushConstants;

void main() {
    vec4 texAlbedo = texture(albedo_map, texCoord);
    vec4 texRoughness = texture(rougness_map, texCoord);
    vec4 texNormal = texture(normal_map, texCoord);

    if (texAlbedo.a < PushConstants.AlphaCutoff.x)
        discard;

    vec4 res = texAlbedo;

    vec3 normal = 2.0 * texNormal.xyz - 1.0;

    vec3 N = vertNormal;
    vec3 T = vertTangent.xyz;
    vec3 B = vertTangent.w * cross(N,T);

    mat3 TBN = mat3(T,B,N);

    normal = normalize(TBN * normal);

    const vec3 ldir = normalize(vec3(1,-1,1));
    float dif = dot(ldir, normal);
    dif = clamp(dif, 0.0, 1.0);

    res.rgb *= 0.1 + 0.9*dif;
    res.rgb = pow(res.rgb, vec3(0.9));

    outColor = res;
}