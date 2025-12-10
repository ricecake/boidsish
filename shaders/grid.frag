#version 330 core
out vec4 FragColor;

in vec3 WorldPos;

void main()
{
    float grid_spacing = 1.0;

    vec2 coord = WorldPos.xz / grid_spacing;
    vec2 f = fwidth(coord);

    // Minor grid lines
    vec2 grid_minor = abs(fract(coord - 0.5) - 0.5) / f;
    float line_minor = min(grid_minor.x, grid_minor.y);
    float C_minor = 1.0 - min(line_minor, 1.0);

    // Major grid lines (every 5th)
    // The division by 5 makes the triangle wave 5 times wider, resulting in a thicker line.
    vec2 grid_major = abs(fract(coord / 5.0 - 0.5) - 0.5) / f;
    float line_major = min(grid_major.x, grid_major.y);
    float C_major = 1.0 - min(line_major, 1.0);

    // Combine intensities: major lines are brighter
    float intensity = max(C_minor, C_major * 1.5);

    // Gentle fade into the distance
    float dist = length(WorldPos.xz);
    float fade_start = 80.0;
    float fade_end = 120.0;
    float fade = 1.0 - smoothstep(fade_start, fade_end, dist);

    FragColor = vec4(0.0, 0.8, 0.8, intensity * fade);
}
