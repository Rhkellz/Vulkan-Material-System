//glsl version 4.5
#version 450
#extension GL_KHR_vulkan_glsl : enable

//shader input
layout (location = 0) in vec3 in_col;
layout (location = 1) in vec2 in_uv;
layout (location = 2) in vec3 in_normal;
//output write
layout (location = 0) out vec4 out_frag_color;

layout(set = 0, binding = 0) uniform sampler2D display_texture;

layout(set = 1, binding = 0) uniform Scene_data {
    mat4 model;
	vec4 light_dir; // w for sun power
	vec4 light_col;
} scene_data;

void main() 
{
	vec4 tex = texture(display_texture,in_uv);

	vec3 col = tex.xyz;
	float alpha = tex.a;

	col = clamp(dot(in_normal, scene_data.light_dir.xyz), 0.0, 1.0) * col;

	out_frag_color = vec4(col, alpha);
}