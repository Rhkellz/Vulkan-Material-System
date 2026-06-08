#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_EXT_buffer_reference : require

layout( push_constant ) uniform constants
{	
	mat4 render_matrix;
	mat4 model_matrix;
} PushConstants;

layout(set = 0, binding = 0) uniform samplerCube skybox_tex;


layout(location = 0) in vec3 in_tex_coords;

layout (location = 0) out vec4 out_frag_color;

void main() {
	out_frag_color = vec4(texture(skybox_tex, in_tex_coords).xyz, 1);
}