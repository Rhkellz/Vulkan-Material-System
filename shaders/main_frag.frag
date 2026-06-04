//glsl version 4.5
#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

#include "input_structures.glsl"

//shader input
layout (location = 0) in vec3 in_col;
layout (location = 1) in vec2 in_uv;
layout (location = 2) in vec3 in_normal;
layout (location = 3) in vec4 in_tangent;
layout (location = 4) in vec4 in_world_pos;
//output write
layout (location = 0) out vec4 out_frag_color;


vec3 calc_mapped_normal(vec3 normal, vec3 tangent, float handedness) {
    vec3 N = normalize(normal);
    vec3 T = normalize(tangent - dot(tangent, N) * N);
    
    vec3 B = cross(N, T) * handedness; 

    vec3 bumpMapNormal = texture(normal_map_tex, in_uv).xyz;
    bumpMapNormal = bumpMapNormal * 2.0 - 1.0;

    mat3 TBN = mat3(T, B, N);
    vec3 worldSpaceNormal = TBN * bumpMapNormal;

    return normalize(worldSpaceNormal);
}

float GGX_distribution(float n_dot_h, float roughness) {
    float r_square = roughness*roughness;

    if (n_dot_h <= 0.0) {
       return 0.0; 
    }

    float piece = (n_dot_h*n_dot_h * (r_square - 1.0) + 1.0);

    return r_square / (pi * (piece*piece));
}

vec3 schlick_fresnel(vec3 V, vec3 H, vec3 F_0) {

    return F_0 + (vec3(1.0) - F_0) * pow(1.0 - max(0.0, dot(V, H)), 5.0);
}

float geometry_schlick_ggx(float n_dot_x, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = n_dot_x;
    float denom = n_dot_x * (1.0 - k) + k;

    return num / denom;
}

float geometry_smith(vec3 n, vec3 V, vec3 L, float roughness) {
    float n_dot_v = max(0.0, dot(n, V));
    float n_dot_l = max(0.0, dot(n, L));
    
    float ggx1 = geometry_schlick_ggx(n_dot_v, roughness);
    float ggx2 = geometry_schlick_ggx(n_dot_l, roughness);

    return ggx1 * ggx2;
}

vec3 BRDF(vec3 n, vec3 L, vec3 V, vec3 H, vec3 albedo, float roughness, vec3 F_0) {
    vec3 diffuse = albedo / pi;
    float denom = 4.0 * max(0.0, dot(n, L)) * max(0.0, dot(n, V)) + 0.001;

    return diffuse + vec3(GGX_distribution(dot(n, H), roughness) * schlick_fresnel(V, H, F_0) * geometry_smith(n, V, L, roughness) / denom);
}

void main() {
    
    vec4 tex_sample = vec4(1.0);

    if ((PushConstants.flags & 0x1) != 0) {
        tex_sample = texture(albedo_tex, in_uv);
    }

    vec3 normal = in_normal;
    if ((PushConstants.flags & 0x2) != 0) {
        normal = calc_mapped_normal(in_normal, in_tangent.xyz, in_tangent.w);
    }

    float roughness = 1.0;
    if ((PushConstants.flags & 0x4) != 0) {
        roughness = texture(roughness_tex, in_uv).x;
    }

    vec3 albedo = tex_sample.xyz;
    float alpha = tex_sample.a;
    


    vec3 V = normalize(scene_data.cam_pos.xyz - in_world_pos.xyz);
    vec3 L = normalize(-scene_data.light_dir.xyz); 
    vec3 H = normalize(L + V);

    float metallic = 0.0;
    
    if ((PushConstants.flags & 0x8) != 0) {
        metallic = texture(metalness_tex, in_uv).x;
    }
    vec3 dielectric_F_0 = vec3(0.04, 0.04, 0.04);

    vec3 active_albedo = albedo * (1.0 - metallic);

    vec3 F_0 = mix(dielectric_F_0, albedo, metallic);

    // remember to include light col
    vec3 col = pi * BRDF(normal, L, V, H, active_albedo, roughness, F_0) * max(0.0, dot(normal, L));

    out_frag_color = vec4(col, alpha);
}