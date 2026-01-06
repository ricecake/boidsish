#version 430 core

in float f_lifetime;
in vec2 f_uv;
flat in int f_style;
out vec4 FragColor;

void main() {
    vec3 color;
    float alpha;

    if (f_style == 3) { // Tracer
        vec3 hot_color = vec3(1.0, 1.0, 0.8);
        vec3 mid_color = vec3(1.0, 0.5, 0.0);
        vec3 cool_color = vec3(0.8, 0.1, 0.0);

        // Use the U coordinate to create a gradient along the tracer's length
        float color_mix = f_uv.x;
        color = mix(hot_color, mid_color, color_mix);
        color = mix(color, cool_color, f_lifetime / 1.5);

        // Fade out with lifetime
        alpha = f_lifetime / 1.5;
    } else {
        // Shape other particles into a circle
        vec2 circ = f_uv - vec2(0.5);
        float dist_sq = dot(circ, circ);
        if (dist_sq > 0.25) {
            discard;
        }

        // Color logic for other particle types
        if (f_style == 0) { // Rocket Trail
            vec3 hot_color = vec3(0.8, 0.6, 0.6);
            vec3 mid_color = vec3(0.6, 0.3, 0.0);
            vec3 cool_color = vec3(0.2, 0.1, 0.0);
            vec3 smoke_color = vec3(0.1, 0.0, 0.0);
            color = mix(mix(smoke_color, cool_color, f_lifetime/2.5), mix(mid_color, hot_color, f_lifetime/2.5), f_lifetime/5);
            alpha = 1.0 - length(color - smoke_color);
        } else if (f_style == 1) { // Explosion
            vec3 hot_color = vec3(1.0, 1.0, 0.8);
            vec3 mid_color = vec3(0.8, 0.6, 0.0);
            vec3 cool_color = vec3(0.2, 0.1, 0.0);
            vec3 smoke_color = vec3(0.1, 0.0, 0.0);
            color = mix(mix(smoke_color, cool_color, f_lifetime/5), mix(mid_color, hot_color, f_lifetime/5), f_lifetime/10);
            alpha = smoothstep(0.25, 0.75, f_lifetime/2.5);
        } else { // Default Fire
            vec3 hot_color = vec3(0.7, 0.7, 0.4);
            vec3 mid_color = vec3(0.7, 0.5, 0.0);
            vec3 cool_color = vec3(0.4, 0.1, 0.0);
            vec3 smoke_color = vec3(0.2, 0.2, 0.0);
            color = mix(mix(smoke_color, cool_color, f_lifetime/2.5), mix(mid_color, hot_color, f_lifetime/2.5), f_lifetime/10.5);
            alpha = smoothstep(0.25, 0.75, f_lifetime/2.5);
        }
    }

    if (f_style == 28) {
		// --- Iridescence Effect ---
        vec2 circ = f_uv - vec2(0.5);
        float dist_sq = dot(circ, circ);
		float fresnel = pow(1.0 - dist_sq, 5.0);
		float angle_factor = 1.0 - dist_sq;
		angle_factor = pow(angle_factor, 2.0);
		float swirl = sin(f_lifetime * 0.5 + f_uv.y * 2.0) * 0.5 + 0.5;
		vec3 iridescent_color = vec3(
			sin(angle_factor * 10.0 + swirl * 5.0) * 0.5 + 0.5,
			sin(angle_factor * 10.0 + swirl * 5.0 + 2.0) * 0.5 + 0.5,
			sin(angle_factor * 10.0 + swirl * 5.0 + 4.0) * 0.5 + 0.5
		);
        // This is a bit of a hack since we don't have view_pos anymore
		vec3  reflect_dir = reflect(vec3(0,0,-1), vec3(dist_sq, dist_sq, dist_sq)).xyz;
		float spec = pow(max(dot(vec3(0,0,1), reflect_dir), 0.0), 128.0);
		vec3  specular = 1.5 * spec * vec3(1.0);
		vec3 final_color = mix(iridescent_color, vec3(1.0), fresnel) + specular;
		FragColor = vec4(final_color, 0.75);
        return;
    }


    FragColor = vec4(color, alpha);

}