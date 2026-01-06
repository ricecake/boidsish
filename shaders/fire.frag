#version 430 core

in float v_lifetime;
in vec4 view_pos;
in vec3 v_velocity;
in vec3 v_view_velocity;
in vec3 v_view_dir;
in vec2 v_screen_vel_dir;
flat in int v_style;
out vec4 FragColor;

void main() {
    vec2 p = gl_PointCoord - vec2(0.5);
    float dist_sq = dot(p, p);

    if (v_style == 3) { // Tracer
        // Foreshortening based on view angle (world space)
        float foreshortening = 1.0 - abs(dot(normalize(v_velocity), v_view_dir));
        float speed_stretch = 1.0 + length(v_velocity) * 0.05;
        float final_stretch = speed_stretch * foreshortening;

        // Rotate point coordinate to align with screen-space velocity
        mat2 rot = mat2(v_screen_vel_dir.x, -v_screen_vel_dir.y, v_screen_vel_dir.y, v_screen_vel_dir.x);
        vec2 rotated_p = rot * p;

        // Stretch and narrow the shape
        rotated_p.x *= final_stretch;
        rotated_p.y *= 0.5 / max(final_stretch, 0.1); // Ensure it doesn't get too wide

        float new_dist_sq = dot(rotated_p, rotated_p);
        if (new_dist_sq > 0.25) {
            discard;
        }

        vec3 hot_color = vec3(1.0, 1.0, 0.8);
        vec3 mid_color = vec3(1.0, 0.5, 0.0);
        vec3 cool_color = vec3(0.8, 0.1, 0.0);

        float color_mix = smoothstep(0.0, 0.25, rotated_p.x + 0.5);
        vec3 color = mix(hot_color, mid_color, color_mix);
        color = mix(color, cool_color, v_lifetime / 1.5);

        float alpha = (1.0 - new_dist_sq / 0.25) * (v_lifetime / 1.5);
        FragColor = vec4(color, alpha);
        return;
    }


    // Shape the point into a circle and discard fragments outside the circle
    if (dist_sq > 0.25) {
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
        vec3 hot_color = vec3(0.7, 0.7, 0.4);   // Bright yellow-white
        vec3 mid_color = vec3(0.7, 0.5, 0.0);   // Orange
        vec3 cool_color = vec3(0.4, 0.1, 0.0);    // Dark red/smokey
        vec3 smoke_color = vec3(0.2, 0.2, 0.0);    // Dark red/smokey
        color = mix(mix(smoke_color, cool_color, v_lifetime/2.5), mix(mid_color, hot_color, v_lifetime/2.5), v_lifetime/10.5);
    }

    if (v_style == 28) {
		// --- Iridescence Effect ---
		// Fresnel term for the base reflectivity
		float fresnel = pow(1.0 - dist_sq, 5.0);

		// Use view angle to create a color shift
		float angle_factor = 1.0 - dist_sq;
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
		vec3  reflect_dir = reflect(-view_pos+vec4(0,20,0,0), vec4(dist_sq)).xyz;
		float spec = pow(max(dot(view_pos.xyz, reflect_dir), 0.0), 128.0);
		vec3  specular = 1.5 * spec * vec3(1.0); // white highlight

		vec3 final_color = mix(iridescent_color, vec3(1.0), fresnel) + specular;

		FragColor = vec4(final_color, 0.75); // Semi-transparent
    } else if (v_style == 0) {
        float alpha = 1 - length(color - vec3(0.1, 0.0, 0.0)); // whatever smoke color is
        FragColor = vec4(color, alpha);
	} else {
        // float alpha = smoothstep((1.0 - v_lifetime), (v_lifetime), dist_sq*v_lifetime / 2.5);
        // float alpha = smoothstep(0.25, 0.75, dist_sq * v_lifetime);
        float alpha = smoothstep(0.25, 0.75, v_lifetime/2.5);
        FragColor = vec4(color, alpha);
    }

}