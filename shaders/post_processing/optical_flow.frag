#version 420 core

in vec2 v_tex_coords;

layout(location = 0) out vec4 out_acceleration;
layout(location = 1) out vec4 out_flow;

uniform sampler2D u_current_texture;
uniform sampler2D u_previous_texture;
uniform sampler2D u_previous_flow_texture;

uniform float u_lambda = 0.01;
uniform float u_offset = 1.0;
uniform vec2  u_texel_size;

// Calculates luminance of a color
float luma(vec3 color) {
	return dot(color, vec3(0.299, 0.587, 0.114));
}

vec2 calc_optical_flow(vec2 uv) {
	vec2 offset_vec = u_offset * u_texel_size;

	// Gradient in X
	float gradX = luma(texture(u_current_texture, uv + offset_vec.xy).rgb) -
		luma(texture(u_current_texture, uv - offset_vec.xy).rgb);
	gradX += luma(texture(u_previous_texture, uv + offset_vec.xy).rgb) -
		luma(texture(u_previous_texture, uv - offset_vec.xy).rgb);

	// Gradient in Y
	float gradY = luma(texture(u_current_texture, uv + offset_vec.yx).rgb) -
		luma(texture(u_current_texture, uv - offset_vec.yx).rgb);
	gradY += luma(texture(u_previous_texture, uv + offset_vec.yx).rgb) -
		luma(texture(u_previous_texture, uv - offset_vec.yx).rgb);

	// Temporal difference
	float diff = luma(texture(u_current_texture, uv).rgb) - luma(texture(u_previous_texture, uv).rgb);

	float grad_mag_sq = gradX * gradX + gradY * gradY + u_lambda;

	return -vec2(diff * (gradX / grad_mag_sq), diff * (gradY / grad_mag_sq));
}

void main() {
	// Calculate the optical flow for the current frame
	vec2 current_flow = calc_optical_flow(v_tex_coords);

	// Get the optical flow from the previous frame
	vec2 previous_flow = texture(u_previous_flow_texture, v_tex_coords).rg;

	// Calculate the difference (acceleration)
	vec2  acceleration = current_flow - previous_flow;
	float acceleration_magnitude = length(acceleration); // Scale for visibility

	// Output the visual representation of acceleration
	vec4 tex = texture(u_current_texture, v_tex_coords);
	// out_acceleration = vec4(fwidth(current_flow.x), fwidth(current_flow.y), fwidth(previous_flow.x),
	// fwidth(previous_flow.y));
	out_acceleration = vec4(tex.rgb, mix(0.01, tex.a, acceleration_magnitude));
	// out_acceleration = vec4(tex.rgb, mix(0.01, tex.a, 1-length(current_flow/(acceleration))));
	// out_acceleration = vec4(tex.rgb, mix(0.01, tex.a, acceleration_magnitude));
	// out_acceleration = vec4(tex.rgb, mix(0.01, tex.a, length(current_flow/previous_flow)));
	// out_acceleration = vec4(texture(u_current_texture, v_tex_coords).rgb, 1.0) +
	// vec4(vec3(acceleration_magnitude), 1.0); out_acceleration = vec4(vec3(acceleration_magnitude), 1.0);

	// Output the current flow to be used in the next frame
	out_flow = vec4(current_flow, 0.0, 1.0);
}
