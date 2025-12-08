#version 330 core
out vec4 FragColor;

in vec3 FragPos;

uniform vec3 iResolution;
uniform mat4 inverseViewMatrix;
uniform vec3 cameraPosition;

float grid(vec2 coord, float thickness) {
    vec2 grid_coord = abs(fract(coord - 0.5) - 0.5) / fwidth(coord);
    float line = min(grid_coord.x, grid_coord.y);
    return 1.0 - min(line, thickness);
}

void main() {
    vec3 view_dir = normalize(vec3(inverseViewMatrix * vec4(FragPos, 0.0)));

    // Sky gradient
    float horizon_intensity = 1.0 - abs(view_dir.y);
    vec3 sky_color = vec3(0.0, 0.0, 0.0);
    vec3 horizon_color = vec3(0.8, 0.2, 0.05);
    float glow_width = 0.2;
    float glow = smoothstep(0.0, glow_width, horizon_intensity) - smoothstep(glow_width, 0.4, horizon_intensity);
    sky_color = mix(sky_color, horizon_color, glow);

    // Grid plane intersection
    float t = -cameraPosition.y / view_dir.y;
    vec3 world_pos = cameraPosition + t * view_dir;

uniform vec3 gridColor;

    // Grid
    float fade_dist = 50.0;
    float dist = distance(cameraPosition, world_pos);
    float fade = 1.0 - smoothstep(fade_dist, fade_dist * 2.0, dist);

    float grid_val = 0.0;
    if (t > 0.0) {
        float major_grid = grid(world_pos.xz / 10.0, 1.0);
        float minor_grid = grid(world_pos.xz, 0.5);
        grid_val = max(major_grid * 0.2, minor_grid * 0.1);
        grid_val *= fade;
    }

    vec3 final_color = mix(sky_color, gridColor, grid_val);
    FragColor = vec4(final_color, 1.0);
}
