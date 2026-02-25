#version 450

#extension GL_EXT_buffer_reference : require

// TODO: restore this as separate codepath:
//layout(location = 0) in vec3 aPosition;
//layout(location = 1) in vec2 aTexCoord;

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

layout(location = 0) out vec2 texCoord;

layout(push_constant) uniform constants {
    mat4 LightMVP;
    VertexBuffer VertBuff;
} PushConstants;

void main() {
    //vec3 position = aPosition;
    //vec2 texcoord = aTexCoord;

    Vertex vert = PushConstants.VertBuff.Vertices[gl_VertexIndex];
    vec3 position = vert.Position;
    vec2 texcoord = vec2(vert.TexCoordX, vert.TexCoordY);
    
    gl_Position = PushConstants.LightMVP * vec4(position, 1.0);

    texCoord = texcoord;
}