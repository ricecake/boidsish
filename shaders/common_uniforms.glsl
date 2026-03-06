struct CommonUniforms {
	mat4  model;
	vec4  color;

	float roughness;
	float metallic;
	float ao;
	int   use_pbr;

	int use_texture;
	int is_line;
	int line_style;
	int is_text_effect;

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

	vec4 dissolve_plane;

	int dissolve_enabled;
	int bone_matrices_offset;
	int use_skinning;
	int padding_skeletal;

	float aabb_min_x;
	float aabb_min_y;
	float aabb_min_z;
	float aabb_max_x;

	float aabb_max_y;
	float aabb_max_z;
	float wind_responsiveness;
	float wind_rim_highlight;

	uvec2 diffuse_handle;
	uvec2 normal_handle;
	uvec2 specular_handle;
	uvec2 padding_handles;

	uvec2 padding_end[29];
};
