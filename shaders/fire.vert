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

    // Set point size based on lifetime
    // Particles will be smaller as they age and die out
    gl_PointSize = (v_lifetime * v_lifetime) * 25.0;
}
