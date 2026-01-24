#version 430 core

in float f_lifetime;
in vec2  f_tex_coord;
flat in int   f_style;
out vec4 FragColor;

void main() {
	// Shape the point into a circle and discard fragments outside the circle
	vec2  circ = f_tex_coord - vec2(0.5);
	float dist = dot(circ, circ);
	if (dist > 0.25) {
		discard;
	}

	vec3 color;
	if (f_style == 0) {                       // Rocket Trail
		vec3 hot_color = vec3(0.8, 0.6, 0.6); // Bright blue-white
		vec3 mid_color = vec3(0.6, 0.3, 0.0);   // Orange
		vec3 cool_color = vec3(0.2, 0.1, 0.0);  // Dark red/smokey
		vec3 smoke_color = vec3(0.1, 0.0, 0.0); // Dark red/smokey

		color = mix(
			mix(smoke_color, cool_color, f_lifetime / 2.5),
			mix(mid_color, hot_color, f_lifetime / 2.5),
			f_lifetime / 5
		);
	} else if (f_style == 1) { // Explosion
		vec3 hot_color = vec3(1.0, 1.0, 0.8);
		vec3 mid_color = vec3(0.8, 0.6, 0.0);   // Orange
		vec3 cool_color = vec3(0.2, 0.1, 0.0);  // Dark red/smokey
		vec3 smoke_color = vec3(0.1, 0.0, 0.0); // Dark red/smokey

		color = mix(
			mix(smoke_color, cool_color, f_lifetime / 5),
			mix(mid_color, hot_color, f_lifetime / 5),
			f_lifetime / 10
		);
	} else if (f_style == 2) {                  // Default Fire
		vec3 hot_color = vec3(0.7, 0.7, 0.4);   // Bright yellow-white
		vec3 mid_color = vec3(0.7, 0.5, 0.0);   // Orange
		vec3 cool_color = vec3(0.4, 0.1, 0.0);  // Dark red/smokey
		vec3 smoke_color = vec3(0.2, 0.2, 0.0); // Dark red/smokey
		color = mix(
			mix(smoke_color, cool_color, f_lifetime / 2.5),
			mix(mid_color, hot_color, f_lifetime / 2.5),
			f_lifetime / 10.5
		);
	}

	if (f_style == 0) {
		float alpha = 1 - length(color - vec3(0.1, 0.0, 0.0)); // whatever smoke color is
		FragColor = vec4(color, alpha);
	} else {
		float alpha = smoothstep(0.25, 0.75, f_lifetime / 2.5);
		FragColor = vec4(color, alpha);
	}
}
