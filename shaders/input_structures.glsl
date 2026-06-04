#define pi 3.14159

layout(set = 1, binding = 0) uniform Scene_data {
	vec4 light_dir; // w for sun power
	vec4 light_col;
	vec4 cam_pos;
} scene_data;


struct Vertex {
	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
	vec4 tangent;
}; 

layout(buffer_reference, std430) readonly buffer Vertex_buffer{ 
	Vertex vertices[];
};

layout( push_constant ) uniform constants
{	
	mat4 render_matrix;
	mat4 model_matrix;
	uint flags;
	Vertex_buffer vertex_buffer;
} PushConstants;

layout(set = 0, binding = 0) uniform sampler2D albedo_tex;
layout(set = 0, binding = 1) uniform sampler2D normal_map_tex;
layout(set = 0, binding = 2) uniform sampler2D roughness_tex;
layout(set = 0, binding = 3) uniform sampler2D metalness_tex;