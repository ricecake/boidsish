#include <iostream>
#include <glm/glm.hpp>

namespace Boidsish {
	struct CommonUniforms {
		glm::mat4 model = glm::mat4(1.0f); // 64 bytes
		glm::vec4 color = glm::vec4(1.0f); // 16 bytes (xyz=color, w=alpha)

		// Material/PBR
		int   use_pbr = 0;      // 4 bytes
		float roughness = 0.5f; // 4 bytes
		float metallic = 0.0f;  // 4 bytes
		float ao = 1.0f;        // 4 bytes -> 16 bytes

		// Feature Flags
		int use_texture = 0;    // 4 bytes
		int is_line = 0;        // 4 bytes
		int line_style = 0;     // 4 bytes
		int is_text_effect = 0; // 4 bytes -> 16 bytes

		// Text/Arcade Effects
		float text_fade_progress = 1.0f; // 4 bytes
		float text_fade_softness = 0.1f; // 4 bytes
		int   text_fade_mode = 0;        // 4 bytes
		int   is_arcade_text = 0;        // 4 bytes -> 16 bytes

		int   arcade_wave_mode = 0;          // 4 bytes
		float arcade_wave_amplitude = 0.5f;  // 4 bytes
		float arcade_wave_frequency = 10.0f; // 4 bytes
		float arcade_wave_speed = 5.0f;      // 4 bytes -> 16 bytes

		int   arcade_rainbow_enabled = 0;      // 4 bytes
		float arcade_rainbow_speed = 2.0f;     // 4 bytes
		float arcade_rainbow_frequency = 5.0f; // 4 bytes
		int   checkpoint_style = 0;            // 4 bytes -> 16 bytes

		// Rendering State Flags
		int   is_colossal = 0;          // 4 bytes
		int   use_ssbo_instancing = 0;  // 4 bytes
		int   use_vertex_color = 0;     // 4 bytes
		float checkpoint_radius = 0.0f; // 4 bytes -> 16 bytes

		// Dissolve Effects
		glm::vec3 dissolve_plane_normal = glm::vec3(0, 1, 0); // 12 bytes
		float     dissolve_plane_dist = 0.0f;                 // 4 bytes -> 16 bytes
		int       dissolve_enabled = 0;                       // 4 bytes

		// Skeletal Animation
		int   bone_matrices_offset = -1; // 4 bytes
		int   use_skinning = 0;          // 4 bytes
		float anim_padding[2];           // 8 bytes -> 16 bytes

		// Occlusion culling AABB (world space) - individual floats for std430 alignment safety
		float aabb_min_x = 0.0f;   // 4 bytes
		float aabb_min_y = 0.0f;   // 4 bytes
		float aabb_min_z = 0.0f;   // 4 bytes
		float aabb_max_x = 0.0f;   // 4 bytes -> 16
		float aabb_max_y = 0.0f;   // 4 bytes
		float aabb_max_z = 0.0f;   // 4 bytes
		int   is_refractive = 0;   // 4 bytes
		float refractive_index = 1.0f; // 4 bytes
		// Padding to 256 bytes for SSBO alignment safety
		float padding[3];
	};
}

using namespace Boidsish;

int main() {
    std::cout << "Size of CommonUniforms: " << sizeof(CommonUniforms) << std::endl;
    return 0;
}
