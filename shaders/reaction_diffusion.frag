#version 420 core

out vec2 fragColor;

uniform sampler2D u_texture;
uniform vec2 u_resolution;

uniform float u_feed_rate;
uniform float u_kill_rate;
uniform float u_diffuse_a;
uniform float u_diffuse_b;
uniform float u_timestep;

vec2 laplacian(vec2 uv) {
    vec2 sum = vec2(0.0);

    // Sample neighbors
    sum += texture(u_texture, uv + vec2(-1.0, 0.0) / u_resolution).rg * 0.20;
    sum += texture(u_texture, uv + vec2(1.0, 0.0) / u_resolution).rg  * 0.20;
    sum += texture(u_texture, uv + vec2(0.0, -1.0) / u_resolution).rg * 0.20;
    sum += texture(u_texture, uv + vec2(0.0, 1.0) / u_resolution).rg  * 0.20;

    sum += texture(u_texture, uv + vec2(-1.0, -1.0) / u_resolution).rg * 0.05;
    sum += texture(u_texture, uv + vec2(1.0, -1.0) / u_resolution).rg  * 0.05;
    sum += texture(u_texture, uv + vec2(-1.0, 1.0) / u_resolution).rg  * 0.05;
    sum += texture(u_texture, uv + vec2(1.0, 1.0) / u_resolution).rg   * 0.05;

    // Subtract center sample
    sum -= texture(u_texture, uv).rg;

    return sum;
}

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution;
    vec2 val = texture(u_texture, uv).rg;

    float a = val.x;
    float b = val.y;

    vec2 laplace = laplacian(uv);

    float reaction = a * b * b;

    float da = u_diffuse_a * laplace.x - reaction + u_feed_rate * (1.0 - a);
    float db = u_diffuse_b * laplace.y + reaction - (u_kill_rate + u_feed_rate) * b;

    a += da * u_timestep;
    b += db * u_timestep;

    fragColor = vec2(clamp(a, 0.0, 1.0), clamp(b, 0.0, 1.0));
}
