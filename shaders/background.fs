#version 330 core
out vec4 FragColor;

in vec2 screenCoord;

uniform mat4 inverseProjectionMatrix;
uniform mat4 inverseViewMatrix;
uniform vec3 cameraPosition;
uniform vec3 gridColor;

float grid(vec2 coord, float thickness) {
    vec2 grid_coord = abs(fract(coord - 0.5) - 0.5) / fwidth(coord);
    float line = min(grid_coord.x, grid_coord.y);
    return 1.0 - min(line, thickness);
}

// 2D Random function
float random(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

void main() {
    vec4 ray_clip = vec4(screenCoord.x, screenCoord.y, -1.0, 1.0);
    vec4 ray_view = inverseProjectionMatrix * ray_clip;
    ray_view = vec4(ray_view.xy, -1.0, 0.0);
    vec3 view_dir = normalize(vec3(inverseViewMatrix * ray_view));

    // Sky gradient
    vec3 sky_color = vec3(0.0, 0.0, 0.0);
    vec3 horizon_color = vec3(0.8, 0.2, 0.05);
    float glow_width = 0.3;
    float glow_falloff = 0.4;

    // Correct horizon calculation
    float horizon_angle = 1.0 - acos(view_dir.y)/1.57079632679;

    // Mottled glow
    float noise = random(view_dir.xz * 10.0);
    float mottled_glow = smoothstep(0.0, glow_width, horizon_angle) - smoothstep(glow_width, glow_width + glow_falloff, horizon_angle);
    mottled_glow *= (0.7 + noise * 0.3);
    sky_color = mix(sky_color, horizon_color, mottled_glow);

    // Grid plane intersection
    float t = -cameraPosition.y / view_dir.y;
    vec3 world_pos = cameraPosition + t * view_dir;

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
