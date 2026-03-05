#version 450

#extension GL_EXT_buffer_reference : require

// TODO: restore this as separate codepath:
//layout(location = 0) in vec3 aPosition;

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

layout(push_constant) uniform constants {
    mat4 LightMVP;
    VertexBuffer VertBuff;
} PushConstants;

void main() {
    Vertex vert = PushConstants.VertBuff.Vertices[gl_VertexIndex];
    
    //vec3 position = vert.Position;

    const float normUint16 = 1.0 / 65535.0;

    vec3 position = normUint16 * vec3(vert.PositionX, vert.PositionY, vert.PositionZ);
    position = 2.0 * position - 1.0;

    vec2 texcoord = normUint16 * vec2(vert.TexCoordX, vert.TexCoordY);

    gl_Position = PushConstants.LightMVP * vec4(position, 1.0);
}