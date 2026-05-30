//glsl version 4.5
#version 450
#extension GL_KHR_vulkan_glsl : enable

//shader input
layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;
//output write
layout (location = 0) out vec4 outFragColor;

layout(set =0, binding = 0) uniform sampler2D displayTexture;

void main() 
{
	vec3 light_loc = vec3(1.0, 1.0, 1.0);

	vec4 tex = texture(displayTexture,inUV);
	vec3 col = tex.xyz;
	float alpha = tex.a;

	col = clamp(dot(inNormal, light_loc), 0.0, 1.0)* col;

	outFragColor = vec4(col, alpha);
}