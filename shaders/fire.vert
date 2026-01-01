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
