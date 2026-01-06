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

// To Geometry Shader
out vec3 v_pos;
out vec3 v_vel;
out float v_lifetime;
out float v_point_size;
flat out int v_style;

void main() {
    Particle p = particles[gl_VertexID];

    // Pass particle data to the geometry shader
    v_pos = p.pos.xyz;
    v_vel = p.vel.xyz;
    v_lifetime = p.pos.w;
    v_style = p.style;

    // Calculate point size, as it's still needed for non-tracer particles
    if (p.style == 0) { // Rocket Trail
        v_point_size = smoothstep((1.0 - v_lifetime), v_lifetime, v_lifetime / 2.0) * 15.0;
    } else if (p.style == 1) { // Explosion
        v_point_size = (1.0 - (1.0 - v_lifetime) * (1.0 - v_lifetime)) * 30.0;
    } else { // Fire and Tracer (base size)
        v_point_size = smoothstep(2.0 * (1.0 - v_lifetime), v_lifetime, v_lifetime / 2.5) * 25.0;
    }

    // The vertex shader now only outputs points to the geometry shader.
    // gl_Position is minimal as the geometry shader will calculate the final positions.
    gl_Position = vec4(p.pos.xyz, 1.0);
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
        gl_PointSize = smoothstep((1.0 - v_lifetime), v_lifetime, v_lifetime / 2.0) * 15.0; // Smaller, more consistent size
    } else if (u_style == 2) { // Explosion
        gl_PointSize = (1.0 - (1.0 - v_lifetime) * (1.0 - v_lifetime)) * 30.0; // Starts large, shrinks fast
    } else {
        gl_PointSize = 25.0; // Default size
    }
}
*/
