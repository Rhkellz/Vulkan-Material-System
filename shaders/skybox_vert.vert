#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_EXT_buffer_reference : require

layout( push_constant ) uniform constants
{	
	mat4 render_matrix;
	mat4 model_matrix;
} PushConstants;

layout(location = 0) out vec3 out_tex_coords;

void main() 
{
    
    vec3 cube_vertices[8] = vec3[8](
        vec3(-1.0, -1.0,  1.0),
        vec3( 1.0, -1.0,  1.0),
        vec3( 1.0,  1.0,  1.0),
        vec3(-1.0,  1.0,  1.0),
        vec3(-1.0, -1.0, -1.0),
        vec3( 1.0, -1.0, -1.0),
        vec3( 1.0,  1.0, -1.0),
        vec3(-1.0,  1.0, -1.0)
    );

    
    int cube_indices[36] = int[36](
        0, 1, 2, 2, 3, 0, // Front
        1, 5, 6, 6, 2, 1, // Right
        7, 6, 5, 5, 4, 7, // Back
        4, 0, 3, 3, 7, 4, // Left
        4, 5, 1, 1, 0, 4, // Bottom
        3, 2, 6, 6, 7, 3  // Top
    );

    vec3 local_pos = cube_vertices[cube_indices[gl_VertexIndex]];

    out_tex_coords = local_pos;

    vec4 clip_pos = PushConstants.render_matrix * PushConstants.model_matrix * vec4(local_pos, 1.0);

    // 3. CRITICAL FOR DRAWING LAST WITH REVERSE-Z:
    // Force the depth value to be exactly 0.0 (the absolute farthest plane in Reverse-Z)
    // We achieve this by forcing the Z component to 0.0 * clip_pos.w
    gl_Position = vec4(clip_pos.x, clip_pos.y, 0.0, clip_pos.w);
}