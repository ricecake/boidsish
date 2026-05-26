#include <iostream>
#include <glm/glm.hpp>

struct CommonUniforms {
	glm::mat4 model; // 64
	glm::vec4 color; // 16
	int   use_pbr;
	float roughness;
	float metallic;
	float ao;
	int   use_texture;
	int   is_line;
	int   line_style;
	int   is_text_effect;
	float text_fade_progress;
	float text_fade_softness;
	int   text_fade_mode;
	int   is_arcade_text;
	int   arcade_wave_mode;
	float arcade_wave_amplitude;
	float arcade_wave_frequency;
	float arcade_wave_speed;
	int   arcade_rainbow_enabled;
	float arcade_rainbow_speed;
	float arcade_rainbow_frequency;
	int   checkpoint_style;
	int   is_colossal;
	int   use_ssbo_instancing;
	int   use_vertex_color;
	float checkpoint_radius;
	glm::vec3  dissolve_plane_normal;
	float dissolve_plane_dist;
	int   dissolve_enabled;
	int   bone_matrices_offset;
	int   use_skinning;
	float anim_padding[2];
	float aabb_min_x;
	float aabb_min_y;
	float aabb_min_z;
	float aabb_max_x;
	float aabb_max_y;
	float aabb_max_z;
	int   is_refractive;
	float refractive_index;
	float emissive_r;
	float emissive_g;
	float emissive_b;
};

int main() {
    std::cout << "sizeof(CommonUniforms): " << sizeof(CommonUniforms) << std::endl;
    std::cout << "offsetof(emissive_r): " << offsetof(CommonUniforms, emissive_r) << std::endl;
    return 0;
}
