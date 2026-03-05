#version 450

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

// TODO: restore this as separate codepath:
//layout(location = 0) in vec3 aPosition;
//layout(location = 1) in vec2 aTexCoord;
//layout(location = 2) in vec3 aNormal;
//layout(location = 3) in vec4 aTangent;

//struct Vertex {
//	vec3 Position;
//	float TexCoordX;
//	vec3 Normal;
//	float TexCoordY;
//	vec4 Tangent;
//}; 

#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_explicit_arithmetic_types: require

struct Vertex{
    uint16_t PositionX, PositionY, PositionZ;
    uint16_t TexCoordX, TexCoordY;
    uint16_t NormalX, NormalY, NormalZ;
    uint16_t TangentX, TangentY, TangentZ, TangentW;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex Vertices[];
};

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
    VertexBuffer VertBuff;
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
    
    Vertex vert = PushConstants.VertBuff.Vertices[gl_VertexIndex];
    
    //vec3 position = vert.Position;
    //vec2 texcoord = vec2(vert.TexCoordX, vert.TexCoordY);
    //vec3 normal = vert.Normal;
    //vec3 tangent = vert.Tangent.xyz;
    //float bitangentSign = vert.Tangent.w;

    const float normUint16 = 1.0 / 65535.0;

    vec3 position = normUint16 * vec3(vert.PositionX, vert.PositionY, vert.PositionZ);
    position = 2.0 * position - 1.0;

    vec2 texcoord = normUint16 * vec2(vert.TexCoordX, vert.TexCoordY);

    vec3 normal = normUint16 * vec3(vert.NormalX, vert.NormalY, vert.NormalZ);
    normal = 2.0 * normal - 1.0;

    vec3 tangent = normUint16 * vec3(vert.TangentX, vert.TangentY, vert.TangentZ);
    tangent = 2.0 * tangent - 1.0;

    float bitangentSign = 2.0 * float(vert.TangentW) - 1.0;

    gl_Position = MVP * vec4(position, 1.0);

    normal = normalize(adjugate(PushConstants.Model) * normal);
    tangent = vec3(PushConstants.Model * vec4(tangent, 0.0));

    OutData.TexCoord = texcoord;
    OutData.Normal = normal;
    OutData.Tangent = vec4(tangent, bitangentSign);

    OutData.FragPos = vec3(PushConstants.Model * vec4(position, 1.0));
    OutData.FragDistance = length(OutData.FragPos - Ubo.ViewPos);
}