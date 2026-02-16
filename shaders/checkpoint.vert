#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoords;

struct CommonUniforms {
	mat4  model;
	vec4  color;
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
	int   is_instanced;
	int   is_colossal;
	int   use_ssbo_instancing;
	int   use_instance_color;
	int   use_vertex_color;
	float checkpoint_radius;
	float padding[2];
};

layout(std430, binding = 2) buffer UniformsSSBO {
	CommonUniforms uniforms_data[];
};

uniform bool uUseMDI = false;
uniform int  uBaseUniformIndex = 0;

out vec2 TexCoords;
flat out int vUniformIndex;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
	vUniformIndex = uUseMDI ? (uBaseUniformIndex + gl_DrawID) : -1;
	mat4 current_model = uUseMDI ? uniforms_data[vUniformIndex].model : model;

	TexCoords = aTexCoords;
	gl_Position = projection * view * current_model * vec4(aPos, 1.0);
}
