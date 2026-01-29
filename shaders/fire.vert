#version 430 core

// Must match the C++ struct layout in fire_effect.h
struct Particle {
	vec4 pos; // Position (w is lifetime)
	vec4 vel; // Velocity (w is unused)
	int  style;
	int  emitter_index; // Index of the emitter that spawned this particle
	vec2 _padding;      // Explicit padding to align to 16 bytes
};

layout(std430, binding = 0) buffer ParticleBuffer {
	Particle particles[];
};

uniform mat4 u_view;
uniform mat4 u_projection;
uniform vec3 u_camera_pos;

out float    v_lifetime;
out vec4     view_pos;
flat out int v_style;

void main() {
	Particle p = particles[gl_VertexID];

	if (p.pos.w <= 0.0) {
		// Don't draw dead particles
		gl_Position = vec4(-1000.0, -1000.0, -1000.0, 1.0);
		gl_PointSize = 0.0;
		v_style = -1; // A dead particle has no style
	} else {
		view_pos = u_view * vec4(p.pos.xyz, 1.0);
		gl_Position = u_projection * view_pos;
		v_lifetime = p.pos.w;
		v_style = p.style;

		// // Base size on style
		// float base_size = 10.0;
		// if (p.style == 0) { // Exhaust
		//     base_size = 15.0;
		// } else if (p.style == 1) { // Explosion
		//     base_size = 40.0;
		// } else { // Fire
		//     base_size = 25.0;
		// }

		// // Attenuate size by distance and lifetime
		// float distance_factor = 1.0 / (-view_pos.z * 0.1);
		// float lifetime_factor = p.pos.w; // Fades out as lifetime decreases
		// gl_PointSize = base_size * distance_factor * lifetime_factor;

		// Set point size based on lifetime and style
		if (p.style == 0) { // Rocket Trail
			gl_PointSize = smoothstep((1.0 - v_lifetime), v_lifetime, v_lifetime / 2.0) *
				15.0;                                                              // Smaller, more consistent size
		} else if (p.style == 1) {                                                 // Explosion
			gl_PointSize = (1.0 - (1.0 - v_lifetime) * (1.0 - v_lifetime)) * 30.0; // Starts large, shrinks fast
		} else if (p.style == 3) {                                                 // Sparks
			gl_PointSize = 4.0 + v_lifetime * 20.0;
		} else if (p.style == 4) { // Glitter
			gl_PointSize = 6.0;    // Small, consistent square
		} else {
			gl_PointSize = smoothstep(2.0 * (1.0 - v_lifetime), v_lifetime, v_lifetime / 2.5) * 25.0;
		}
	}
}

/*
#version 430 core

// No vertex attributes needed, we'll get data from the SSBO

struct Particle {
    vec4 position; // w component is lifetime
    vec4 velocity;
};

// Corrected SSBO layout
layout(std430, binding = 0) readonly buffer ParticleBuffer {
    Particle particles[];
};

uniform mat4 u_view;
uniform mat4 u_projection;
uniform int u_style;

out float v_lifetime;

void main() {
    // Get the particle for the current vertex
    Particle p = particles[gl_VertexID];

    // Pass lifetime to the fragment shader
    v_lifetime = p.position.w;

    // If the particle is dead, move it off-screen to avoid rendering artifacts
    if (v_lifetime <= 0.0) {
        gl_Position = vec4(-1000.0, -1000.0, -1000.0, 1.0);
    } else {
        // Transform the world position to clip space
        gl_Position = u_projection * u_view * vec4(p.position.xyz, 1.0);
    }

    // Set point size based on lifetime and style
    if (u_style == 0) { // Default Fire
        gl_PointSize = smoothstep(2.0 * (1.0 - v_lifetime), v_lifetime, v_lifetime / 2.5) * 25.0;
    } else if (u_style == 1) { // Rocket Trail
        gl_PointSize = smoothstep((1.0 - v_lifetime), v_lifetime, v_lifetime / 2.0) * 15.0; // Smaller, more consistent
size } else if (u_style == 2) { // Explosion gl_PointSize = (1.0 - (1.0 - v_lifetime) * (1.0 - v_lifetime)) * 30.0; //
Starts large, shrinks fast } else { gl_PointSize = 25.0; // Default size
    }
}
*/
