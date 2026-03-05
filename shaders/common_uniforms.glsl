struct CommonUniforms {
	mat4  model; // [0-63]
	vec4  color; // [64-79]

	// Block 1
	int   use_pbr;
	float roughness;
	float metallic;
	float ao; // [80-95]

	// Block 2
	int   use_texture;
	int   is_line;
	int   line_style;
	int   is_text_effect; // [96-111]

	// Block 3
	float text_fade_progress;
	float text_fade_softness;
	int   text_fade_mode;
	int   is_arcade_text; // [112-127]

	// Block 4
	int   arcade_wave_mode;
	float arcade_wave_amplitude;
	float arcade_wave_frequency;
	float arcade_wave_speed; // [128-143]

	// Block 5
	int   arcade_rainbow_enabled;
	float arcade_rainbow_speed;
	float arcade_rainbow_frequency;
	int   checkpoint_style; // [144-159]

	// Block 6
	int   is_colossal;
	int   use_ssbo_instancing;
	int   use_vertex_color;
	float checkpoint_radius; // [160-175]

	// Block 7
	vec4  dissolve_plane; // [176-191]

	// Block 8
	int   dissolve_enabled;
	int   bone_matrices_offset;
	int   use_skinning;
	float padding_misc; // [192-207]

	// Block 9
	vec4  aabb_min; // [208-223]

	// Block 10
	vec4  aabb_max; // [224-239]

	// Block 11
	float padding_final[4]; // [240-255]
};
