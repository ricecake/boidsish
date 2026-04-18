#version 430 core

#include "helpers/frustum.glsl"
#include "types/occlusion_visibility.glsl"
#include "types/lighting.glsl"
#include "types/shadows.glsl"
#include "types/terrain.glsl"
#include "types/biomes.glsl"
#include "types/particle_types.glsl"

uniform mat4  u_view;
uniform mat4  u_projection;
uniform vec3  u_camera_pos;
uniform bool  enableFrustumCulling = false;
uniform float frustumCullRadius = 1.0;

out float         v_lifetime;
out vec4          view_pos;
out vec4          v_pos;
out float         v_extra[2];
out vec3          v_epicenter;
flat out int      v_style;
flat out int      v_emitter_index;
flat out int      v_emitter_id;
flat out uint     v_particle_idx;
flat out Particle v_p;

void main() {
	uint     particle_idx = visible_indices[gl_VertexID];
	Particle p = particles[particle_idx];
	v_pos = p.pos;
	v_epicenter = p.epicenter;
	v_extra = p.extras;

	{
		view_pos = u_view * vec4(p.pos.xyz, 1.0);
		gl_Position = u_projection * view_pos;
		v_lifetime = p.pos.w;
		v_style = p.style;
		v_emitter_index = p.emitter_index;
		v_emitter_id = p.emitter_id;
		v_particle_idx = particle_idx;

		// Set point size based on lifetime and style
		if (p.style == 0) { // Rocket Trail
			gl_PointSize = smoothstep((1.0 - v_lifetime), v_lifetime, v_lifetime / 2.0) *
				15.0;                                                              // Smaller, more consistent size
		} else if (p.style == 1) {                                                 // Explosion
			gl_PointSize = (1.0 - (1.0 - v_lifetime) * (1.0 - v_lifetime)) * 60.0; // Starts large, shrinks fast
		} else if (p.style == 3) {                                                 // Sparks
			gl_PointSize = 4.0 + v_lifetime * 20.0;
		} else if (p.style == 4) {                                                 // Glitter
			gl_PointSize = 6.0;                                                    // Small, consistent square
		} else if (p.style == 8) {                                                 // Debug
			gl_PointSize = 8.0;                                                    // Fixed size point
		} else if (p.style == 5 || p.style == 6 || p.style == 7 || p.style == 9) { // Ambient, Bubbles, Fireflies,
			                                                                       // Cinder
			// Prominent size but attenuated by distance
			gl_PointSize = 15.0 / (-view_pos.z * 0.05);

			// Vary size by sub-style and random factor
			float size_var = fract(sin(float(particle_idx) * 123.456) * 456.789);
			if (p.style == 6 || (p.style == 5 && v_emitter_id == 2)) { // Bubbles vary more in size
				gl_PointSize *= (0.5 + size_var * 1.5);
			} else if (p.style == 9) { // Cinders are a bit smaller but vary
				gl_PointSize *= (0.6 + size_var * 0.4);
				gl_PointSize *= 0.8;
			} else {
				gl_PointSize *= (0.8 + size_var * 0.4);
			}

			gl_PointSize = clamp(gl_PointSize, 2.0, 40.0);
		} else {
			gl_PointSize = smoothstep(2.0 * (1.0 - v_lifetime), v_lifetime, v_lifetime / 2.5) * 25.0;
		}
	}
}
