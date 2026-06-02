#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outNormal;
layout (location = 3) out vec4 outTangent;

layout(set = 1, binding = 0) uniform Scene_data {
	mat4 model;
	vec4 light_dir; // w for sun power
	vec4 light_col;
} scene_data;


struct Vertex {
	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
	vec4 tangent;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

//layout(set = 0, binding = 0) uniform sampler2D normal_map;

//push constants block
layout( push_constant ) uniform constants
{	
	mat4 render_matrix;
	VertexBuffer vertexBuffer;
} PushConstants;

void main() 
{	
	//load vertex data from device adress
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];

	//output data
	gl_Position = PushConstants.render_matrix * scene_data.model * vec4(v.position, 1.0f);// prebake in model?

	outColor = v.color.xyz;
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;

	outTangent.xyz = mat3(scene_data.model) * v.tangent.xyz;
    outTangent.w = v.tangent.w;

	outNormal = mat3(scene_data.model) * v.normal;// need to normalize if not just rotation matrix
}