#extension GL_EXT_buffer_reference : require

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

const float normUint16 = 1.0 / 65535.0;

vec3 GetPosition(Vertex vert)
{
    vec3 position = normUint16 * vec3(vert.PositionX, vert.PositionY, vert.PositionZ);
    position = 2.0 * position - 1.0;

    return position;
}

vec2 GetTexCoord(Vertex vert)
{
    vec2 texcoord = normUint16 * vec2(vert.TexCoordX, vert.TexCoordY);

    return texcoord;
}

vec3 GetNormal(Vertex vert)
{
    vec3 normal = normUint16 * vec3(vert.NormalX, vert.NormalY, vert.NormalZ);
    normal = 2.0 * normal - 1.0;

    return normal;
}

vec4 GetTangent(Vertex vert)
{
    vec3 tangent = normUint16 * vec3(vert.TangentX, vert.TangentY, vert.TangentZ);
    tangent = 2.0 * tangent - 1.0;

    float bitangentSign = 2.0 * float(vert.TangentW) - 1.0;

    return vec4(tangent, bitangentSign);
}
