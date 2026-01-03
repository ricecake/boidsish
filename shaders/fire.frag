#version 430 core

in float v_lifetime;
in vec4 view_pos;
flat in int v_style;
out vec4 FragColor;

void main() {
    // Shape the point into a circle and discard fragments outside the circle
    vec2 circ = gl_PointCoord - vec2(0.5);
    float dist = dot(circ, circ);
    if (dist > 0.25) {
        discard;
    }

    vec3 color;
    if (v_style == 0) { // Rocket Trail
        vec3 hot_color = vec3(0.8, 0.6, 0.6);   // Bright blue-white
        // vec3 mid_color = vec3(0.5, 0.5, 1.0);   // Blue
        // vec3 cool_color = vec3(0.1, 0.1, 0.3);    // Dark blue
        // vec3 smoke_color = vec3(0.3, 0.3, 0.3);
        // vec3 mid_color = vec3(1.0, 0.5, 0.0);   // Orange
        // vec3 cool_color = vec3(0.4, 0.1, 0.0);    // Dark red/smokey
        // vec3 smoke_color = vec3(0.3, 0.3, 0.3);    // Dark red/smokey

        vec3 mid_color = vec3(0.6, 0.3, 0.0);   // Orange
        vec3 cool_color = vec3(0.2, 0.1, 0.0);    // Dark red/smokey
        vec3 smoke_color = vec3(0.1, 0.0, 0.0);    // Dark red/smokey


        color = mix(mix(smoke_color, cool_color, v_lifetime/2.5), mix(mid_color, hot_color, v_lifetime/2.5), v_lifetime/5);
        // color = vec3(0, 0, 1);
    } else if (v_style == 1) { // Explosion
        vec3 hot_color = vec3(1.0, 1.0, 0.8);
        // vec3 mid_color = vec3(1.0, 0.8, 0.2);
        // vec3 cool_color = vec3(0.8, 0.2, 0.0);
        // vec3 smoke_color = vec3(0.4, 0.4, 0.4);
        vec3 mid_color = vec3(0.8, 0.6, 0.0);   // Orange
        vec3 cool_color = vec3(0.2, 0.1, 0.0);    // Dark red/smokey
        vec3 smoke_color = vec3(0.1, 0.0, 0.0);    // Dark red/smokey

        color = mix(mix(smoke_color, cool_color, v_lifetime/5), mix(mid_color, hot_color, v_lifetime/5), v_lifetime/10);
    } else if (v_style == 2) { // Default Fire
        vec3 hot_color = vec3(1.0, 1.0, 0.6);   // Bright yellow-white
        vec3 mid_color = vec3(1.0, 0.5, 0.0);   // Orange
        vec3 cool_color = vec3(0.4, 0.1, 0.0);    // Dark red/smokey
        vec3 smoke_color = vec3(0.2, 0.2, 0.2);    // Dark red/smokey
        color = mix(mix(smoke_color, cool_color, v_lifetime), mix(mid_color, hot_color, v_lifetime*v_lifetime), v_lifetime);
    }

    if (v_style == 28) {
		// --- Iridescence Effect ---
		// Fresnel term for the base reflectivity
		float fresnel = pow(1.0 - dist, 5.0);

		// Use view angle to create a color shift
		float angle_factor = 1.0 - dist;
		angle_factor = pow(angle_factor, 2.0);

		// Use time and fragment position to create a swirling effect
		float swirl = sin(v_lifetime * 0.5 + gl_PointCoord.y * 2.0) * 0.5 + 0.5;

		// Combine for final color using a rainbow palette shifted by the swirl and angle
		vec3 iridescent_color = vec3(
			sin(angle_factor * 10.0 + swirl * 5.0) * 0.5 + 0.5,
			sin(angle_factor * 10.0 + swirl * 5.0 + 2.0) * 0.5 + 0.5,
			sin(angle_factor * 10.0 + swirl * 5.0 + 4.0) * 0.5 + 0.5
		);

		// Add a strong specular highlight
		vec3  reflect_dir = reflect(-view_pos+vec4(0,20,0,0), vec4(dist)).xyz;
		float spec = pow(max(dot(view_pos.xyz, reflect_dir), 0.0), 128.0);
		vec3  specular = 1.5 * spec * vec3(1.0); // white highlight

		vec3 final_color = mix(iridescent_color, vec3(1.0), fresnel) + specular;

		FragColor = vec4(final_color, 0.75); // Semi-transparent
    } else if (v_style == 0) {
        float alpha = 1 - length(color - vec3(0.3, 0.3, 0.3));
        FragColor = vec4(color, alpha);
	} else {
        // float alpha = smoothstep((1.0 - v_lifetime), (v_lifetime), dist*v_lifetime / 2.5);
        // float alpha = smoothstep(0.25, 0.75, dist * v_lifetime);
        float alpha = smoothstep(0.25, 0.75, v_lifetime/2.5);
        FragColor = vec4(color, alpha);
    }

}