#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform bool colorShift;

void main()
{
    vec3 result = texture(screenTexture, TexCoords).rgb;
    if (colorShift) {
        float shift_magnitude = 0.01;
        float shift_speed = 50.0;
        vec2 red_offset = vec2(sin(TexCoords.y * shift_speed) * shift_magnitude, 0.0);
        vec2 green_offset = vec2(0.0, cos(TexCoords.x * shift_speed) * shift_magnitude);

        float r = texture(screenTexture, TexCoords + red_offset).r;
        float g = texture(screenTexture, TexCoords + green_offset).g;
        float b = texture(screenTexture, TexCoords).b;

        result = vec3(r, g, b);

        int posterize_levels = 8;
		result.r = floor(result.r * posterize_levels) / posterize_levels;
		result.g = floor(result.g * posterize_levels) / posterize_levels;
		result.b = floor(result.b * posterize_levels) / posterize_levels;
    }
    FragColor = vec4(result, 1.0);
}
