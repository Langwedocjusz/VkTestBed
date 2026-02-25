#version 450

#extension GL_EXT_buffer_reference : require

// TODO: restore this as separate codepath:
//layout(location = 0) in vec3 aPosition;

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

layout(push_constant) uniform constants {
    mat4 LightMVP;
    VertexBuffer VertBuff;
} PushConstants;

void main() {
    //vec3 position = aPosition;

    Vertex vert = PushConstants.VertBuff.Vertices[gl_VertexIndex];
    vec3 position = vert.Position;

    gl_Position = PushConstants.LightMVP * vec4(position, 1.0);
}