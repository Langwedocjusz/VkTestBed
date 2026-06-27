#extension GL_EXT_buffer_reference : require

#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_explicit_arithmetic_types: require

struct Vertex{
    uint16_t PositionX, PositionY, PositionZ;
    uint16_t TexCoordX, TexCoordY;
    uint16_t Normal1, Normal2;
    uint16_t Tangent1, Tangent2, BitangentPositive;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex Vertices[];
};

const float normUint16 = 1.0 / 65535.0;

vec3 OctahedralDecode(vec2 v2)
{
    // This puts the vector at the proper z position:
    const float l1 = abs(v2.x) + abs(v2.y);
    vec3 v = vec3(v2.x, v2.y, 1.0 - l1);
    
    // We still need to undo the unwrapping in the xy plane:
    //if (v.z < 0.0)
    vec2 sgn = vec2(greaterThanEqual(v.xy, vec2(0)));
    sgn = 2.0 * sgn - 1.0;

    v.xy -= sgn * clamp(-v.z, 0.0, 1.0);
    
    // And project back to sphere by re-normalizing:
    return normalize(v);
}


vec3 GetPosition(Vertex vert)
{
    vec3 position = normUint16 * vec3(vert.PositionX, vert.PositionY, vert.PositionZ);
    position = 2.0 * position - 1.0;

    return position;
}

vec2 GetTexCoord(Vertex vert)
{
    vec2 texcoord = normUint16 * vec2(vert.TexCoordX, vert.TexCoordY);
    texcoord = 2.0 * texcoord - 1.0;

    return texcoord;
}

vec3 GetNormal(Vertex vert)
{
    vec2 normal2 = normUint16 * vec2(vert.Normal1, vert.Normal2);
    normal2 = 2.0 * normal2 - 1.0;

    vec3 normal = OctahedralDecode(normal2);

    return normal;
}

vec4 GetTangent(Vertex vert)
{
    vec2 tangent2 = normUint16 * vec2(vert.Tangent1, vert.Tangent2);
    tangent2 = 2.0 * tangent2 - 1.0;

    vec3 tangent = OctahedralDecode(tangent2);    

    float bitangentSign = 2.0 * float(vert.BitangentPositive) - 1.0;

    return vec4(tangent, bitangentSign);
}
