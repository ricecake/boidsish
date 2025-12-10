#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform mat4 invProjection;
uniform mat4 invView;

float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

float noise(vec2 p){
    vec2 ip = floor(p); // Integer part
    vec2 u = fract(p);  // Fractional part
    // Smoothstep for smooth interpolation
    u = u*u*(3.0-2.0*u);

    // Get random values for the four corners of the grid cell
    float a = rand(ip);
    float b = rand(ip + vec2(1.0, 0.0));
    float c = rand(ip + vec2(0.0, 1.0));
    float d = rand(ip + vec2(1.0, 1.0));

    // Bilinear interpolation
    return mix(
        mix(a, b, u.x),
        mix(c, d, u.x),
        u.y
    );
}


void main()
{
    // Convert screen coordinates to a world-space direction vector
    vec4 clip = vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
    vec4 view_ray = invProjection * clip;
    vec3 world_ray = (invView * vec4(view_ray.xy, -1.0, 0.0)).xyz;
    world_ray = normalize(world_ray);

    // Color calculations based on the y-component of the view direction
    float y = world_ray.y;

    // Define colors for the gradient
    vec3 twilight_color = vec3(0.9, 0.5, 0.2); // Orangey-red
    vec3 mid_sky_color = vec3(0.2, 0.4, 0.8); // Gentle blue
    vec3 top_sky_color = vec3(0.05, 0.1, 0.3); // Dark blue

    // Blend the colors
    float twilight_mix = smoothstep(0.0, 0.1, y);
    vec3 color = mix(twilight_color, mid_sky_color, twilight_mix);

   // Mottled glow
    color = mix(color, color*0.6, noise(world_ray.xy * 10.0));

    float top_mix = smoothstep(0.1, 0.6, y);
    vec3 top_color = mix(color, top_sky_color, top_mix);

    // space!
    top_color = mix(top_color, color*((noise(world_ray.yz * 10)+noise(world_ray.xy * 10))), top_mix);

    FragColor = vec4(top_color, 1.0);
}
