#version 450

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

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

void main() {
    //vec3 position = aPosition;

    Vertex vert = PushConstants.VertBuff.Vertices[gl_VertexIndex];
    vec3 position = vert.Position;

    mat4 MVP = Ubo.CameraViewProjection * PushConstants.Model;
    
    gl_Position = MVP * vec4(position, 1.0);
}