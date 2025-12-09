#version 330 core
out vec4 FragColor;

in vec3 WorldPos;

uniform float far_plane;

void main()
{
    float grid_spacing = 1.0;
    float line_width = 0.01;

    vec2 coord = WorldPos.xz / grid_spacing;
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / fwidth(coord);
    float line = min(grid.x, grid.y);

    float C = 1.0 - min(line, 1.0);

    float Z = gl_FragCoord.z / gl_FragCoord.w;
    float fade = 1.0 - smoothstep(0.95 * far_plane, far_plane, Z);

    FragColor = vec4(0.0, 0.8, 1.0, C * fade);
}
