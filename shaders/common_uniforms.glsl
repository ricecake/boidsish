struct CommonUniforms {
	mat4  model;
	vec4  color;
	int   use_pbr;
	float roughness;
	float metallic;
	float ao;
	// use_texture is a bitmask:
	// Bit 0 (1): Diffuse, Bit 1 (2): Normal, Bit 2 (4): Metallic,
	// Bit 3 (8): Roughness, Bit 4 (16): AO, Bit 5 (32): Emissive
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
	vec3  dissolve_plane_normal;
	float dissolve_plane_dist;
	int   dissolve_enabled;
	int   bone_matrices_offset;
	int   use_skinning;
	float morph_factor;
	float morph_target_radius;
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
