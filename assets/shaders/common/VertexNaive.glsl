#extension GL_EXT_buffer_reference : require

struct Vertex {
	vec3 Position;
	float TexCoordX;
	vec3 Normal;
	float TexCoordY;
	vec4 Tangent;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex Vertices[];
};

// In this case the functions are totaly trivial and superfluous,
// but I want to have a consistend api between this and the
// compressed variants.

vec3 GetPosition(Vertex vert)
{
    return vert.Position;
}

vec2 GetTexCoord(Vertex vert)
{
    return vec2(vert.TexCoordX, vert.TexCoordY);
}

vec3 GetNormal(Vertex vert)
{
    return vert.Normal;
}

vec4 GetTangent(Vertex vert)
{
    return vert.Tangent;
}