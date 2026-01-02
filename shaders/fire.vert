#version 430 core

// Must match the C++ struct layout in fire_effect.h
struct Particle {
    vec4 pos;           // Position (w is lifetime)
    vec4 vel;           // Velocity (w is unused)
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

out float v_lifetime;
flat out int v_style;

void main() {
    Particle p = particles[gl_VertexID];

    if (p.pos.w <= 0.0) {
        // Don't draw dead particles
        gl_Position = vec4(0.0, 0.0, 0.0, 0.0);
        gl_PointSize = 0.0;
        v_style = -1; // A dead particle has no style
    } else {
        vec4 view_pos = u_view * vec4(p.pos.xyz, 1.0);
        gl_Position = u_projection * view_pos;
        v_lifetime = p.pos.w;
        v_style = p.style;

        // Base size on style
        float base_size = 10.0;
        if (p.style == 0) { // Exhaust
            base_size = 15.0;
        } else if (p.style == 1) { // Explosion
            base_size = 40.0;
        } else { // Fire
            base_size = 25.0;
        }

        // Attenuate size by distance and lifetime
        float distance_factor = 1.0 / (-view_pos.z * 0.1);
        float lifetime_factor = p.pos.w; // Fades out as lifetime decreases
        gl_PointSize = base_size * distance_factor * lifetime_factor;
    }
}