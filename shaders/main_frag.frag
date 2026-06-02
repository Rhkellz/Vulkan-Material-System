//glsl version 4.5
#version 450
#extension GL_KHR_vulkan_glsl : enable

//shader input
layout (location = 0) in vec3 in_col;
layout (location = 1) in vec2 in_uv;
layout (location = 2) in vec3 in_normal;
layout (location = 3) in vec4 in_tangent;
//output write
layout (location = 0) out vec4 out_frag_color;

layout(set = 0, binding = 0) uniform sampler2D albedo;
layout(set = 0, binding = 1) uniform sampler2D normal_map;

layout(set = 1, binding = 0) uniform Scene_data {
    mat4 model;
	vec4 light_dir; // w for sun power
	vec4 light_col;
} scene_data;

vec3 calc_mapped_normal(vec3 normal, vec3 tangent, float handedness) {
    vec3 N = normalize(normal);
    vec3 T = normalize(tangent - dot(tangent, N) * N);
    
    vec3 B = cross(N, T) * handedness; 

    vec3 bumpMapNormal = texture(normal_map, in_uv).xyz;
    bumpMapNormal = bumpMapNormal * 2.0 - 1.0;

    mat3 TBN = mat3(T, B, N);
    vec3 worldSpaceNormal = TBN * bumpMapNormal;

    return normalize(worldSpaceNormal);
}

void main() 
{
	vec4 tex = texture(albedo,in_uv);
	vec3 normal = calc_mapped_normal(in_normal, in_tangent.xyz, in_tangent.w);

	vec3 col = tex.xyz;
	float alpha = tex.a;

	col = clamp(dot(normal, scene_data.light_dir.xyz), 0.0, 1.0) * col;

	out_frag_color = vec4(col, alpha);
}