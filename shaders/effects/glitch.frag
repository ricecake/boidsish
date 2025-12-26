#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform float time;

// Simple pseudo-random function
float random(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

void main() {
    vec2 uv = TexCoords;

    // 1. Blocky Displacement
    float block_speed = 10.0;
    float block_size = 0.05;
    float block_intensity = 0.02;

    float line_noise = random(vec2(time * block_speed, floor(uv.y / block_size) * block_size));
    if (line_noise > 0.96) { // Affect only a small percentage of lines
        float displace = random(vec2(time * block_speed, uv.y)) * block_intensity;
        uv.x += displace;
    }

    // 2. Color Channel Separation (Chromatic Aberration)
    float separation_intensity = 0.01;
    float separation_speed = 20.0;

    float r_offset = (random(vec2(time * separation_speed, 1.0)) - 0.5) * 2.0 * separation_intensity;
    float g_offset = (random(vec2(time * separation_speed, 2.0)) - 0.5) * 2.0 * separation_intensity;
    float b_offset = (random(vec2(time * separation_speed, 3.0)) - 0.5) * 2.0 * separation_intensity;

    float r = texture(sceneTexture, uv + vec2(r_offset, 0.0)).r;
    float g = texture(sceneTexture, uv + vec2(g_offset, 0.0)).g;
    float b = texture(sceneTexture, uv + vec2(b_offset, 0.0)).b;

    // Combine effects
    FragColor = vec4(r, g, b, 1.0);
}
