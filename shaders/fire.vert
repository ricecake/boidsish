#version 460 core
#extension GL_ARB_shader_draw_parameters : enable

#include "frustum.glsl"
#include "lighting.glsl"
#include "particle_types.glsl"

uniform mat4  u_view;
uniform mat4  u_projection;
uniform vec3  u_camera_pos;
uniform bool  enableFrustumCulling = false;
uniform float frustumCullRadius = 1.0;

out float         v_lifetime;
out vec4          view_pos;
out vec4          v_pos;
out vec3          v_vel;
out vec3          v_vel_view;
out vec3          v_origin;
flat out int      v_style;
flat out int      v_emitter_index;
flat out int      v_emitter_id;
flat out uint     v_particle_idx;
flat out Particle v_p;

void main() {
	uint     particle_idx = visible_indices[gl_VertexID];
	Particle p = particles[particle_idx];
	v_pos = p.pos;
	v_vel = p.vel.xyz;
	v_vel_view = (u_view * vec4(p.vel.xyz, 0.0)).xyz;
	v_origin = p.origin.xyz;
	v_style = p.style;
	v_emitter_index = p.emitter_index;
	v_emitter_id = p.emitter_id;
	v_particle_idx = particle_idx;
	v_p = p;

	{
		view_pos = u_view * vec4(p.pos.xyz, 1.0);
		gl_Position = u_projection * view_pos;
		v_lifetime = p.pos.w;

		float base_size = p.vel.w;

		if (p.style == STYLE_RAIN) {
			gl_PointSize = clamp(base_size / (-view_pos.z * 0.1), 2.0, 15.0);
		} else if (p.style == STYLE_SNOW) {
			gl_PointSize = clamp(base_size / (-view_pos.z * 0.1), 4.0, 20.0);
		} else if (p.style == STYLE_AMBIENT || p.style == STYLE_BUBBLES || p.style == STYLE_FIREFLIES || p.style == STYLE_CINDER || p.style == STYLE_LEAF || p.style == STYLE_PETAL || p.style == STYLE_BIRDS) {
			gl_PointSize = base_size / (-view_pos.z * 0.05);

			float size_var = fract(sin(float(particle_idx) * 123.456) * 456.789);
			if (p.style == STYLE_BUBBLES) {
				gl_PointSize *= (0.5 + size_var * 1.5);
			} else if (p.style == STYLE_CINDER) {
				gl_PointSize *= (0.6 + size_var * 0.4) * 0.8;
			} else {
				gl_PointSize *= (0.8 + size_var * 0.4);
			}
			gl_PointSize = clamp(gl_PointSize, 2.0, 40.0);
		} else {
			gl_PointSize = base_size;
		}
	}
}
