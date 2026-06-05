#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

#include "input_structures.glsl"

layout (location = 0) out vec3 out_color;
layout (location = 1) out vec2 out_uv;
layout (location = 2) out vec3 out_normal;
layout (location = 3) out vec4 out_tangent;
layout (location = 4) out vec4 out_world_pos;

void main() 
{	
	//load vertex data from device adress
	Vertex v = PushConstants.vertex_buffer.vertices[gl_VertexIndex];

	out_color = v.color.xyz;
	out_uv.x = v.uv_x;
	out_uv.y = v.uv_y;


	vec3 pos = v.position;

	if ((PushConstants.flags & 0x10) != 0) {
		// Force Lod 0 to stop mipmap selection discrepancies at boundaries
		float height = textureLod(height_tex, out_uv, 0.0).x;
		pos = pos + v.normal * height * PushConstants.displacement_amount;
	}
	

	out_normal = normalize(mat3(PushConstants.model_matrix) * v.normal);

	// Transform Tangent to World Space 
	out_tangent.xyz = normalize(mat3(PushConstants.model_matrix) * v.tangent.xyz);
	out_tangent.w = v.tangent.w; // Preserve the handness sign (+1.0 or -1.0)

	vec4 world_pos = PushConstants.model_matrix * vec4(pos, 1.0f);
	gl_Position = PushConstants.render_matrix * world_pos;
	out_world_pos = world_pos;
}