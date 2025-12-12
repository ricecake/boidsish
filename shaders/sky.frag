#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

layout (std140) uniform Lighting {
    vec3 lightPos;
    vec3 viewPos;
    vec3 lightColor;
    float time;
};


uniform mat4 invProjection;
uniform mat4 invView;

// float rand(vec2 co){
//     return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
// }

// float noise(vec2 p){
//     vec2 ip = floor(p); // Integer part
//     vec2 u = fract(p);  // Fractional part
//     // Smoothstep for smooth interpolation
//     u = u*u*(3.0-2.0*u);

//     // Get random values for the four corners of the grid cell
//     float a = rand(ip);
//     float b = rand(ip + vec2(1.0, 0.0));
//     float c = rand(ip + vec2(0.0, 1.0));
//     float d = rand(ip + vec2(1.0, 1.0));

//     // Bilinear interpolation
//     return mix(
//         mix(a, b, u.x),
//         mix(c, d, u.x),
//         u.y
//     );
// }


// Simplex Noise (2D) - Replace your existing rand and noise functions

// Permutation table (used for hashing the input coordinates)
vec4 permute(vec4 x){return mod(((x*34.0)+1.0)*x, 289.0);}
vec2 fade(vec2 t) {return t*t*t*(t*(t*6.0-15.0)+10.0);}

float snoise(vec2 P){
    vec4 Pi = floor(P.xyxy) + vec4(0.0, 0.0, 1.0, 1.0);
    vec4 Pf = fract(P.xyxy) - vec4(0.0, 0.0, 1.0, 1.0);
    Pi = mod(Pi, 289.0); // Keep values in range
    vec4 ix = Pi.xzxz;
    vec4 iy = Pi.yyww;
    vec4 fx = Pf.xzxz;
    vec4 fy = Pf.yyww;

    vec4 i = permute(permute(ix) + iy);

    vec4 gx = fract(i * (1.0 / 41.0)) * 2.0 - 1.0;
    vec4 gy = abs(gx) - 0.5;
    vec4 tx = floor(gx + 0.5);
    gx = gx - tx;

    vec2 g00 = vec2(gx.x,gy.x);
    vec2 g10 = vec2(gx.y,gy.y);
    vec2 g01 = vec2(gx.z,gy.z);
    vec2 g11 = vec2(gx.w,gy.w);

    vec4 norm = 1.79284291400159 - 0.85373472095314 * vec4(dot(g00, g00), dot(g10, g10), dot(g01, g01), dot(g11, g11));
    g00 *= norm.x;
    g10 *= norm.y;
    g01 *= norm.z;
    g11 *= norm.w;

    float n00 = dot(g00, vec2(fx.x, fy.x));
    float n10 = dot(g10, vec2(fx.y, fy.y));
    float n01 = dot(g01, vec2(fx.z, fy.z));
    float n11 = dot(g11, vec2(fx.w, fy.w));

    vec2 t = fade(Pf.xy);
    return 2.3 * mix(mix(n00, n10, t.x), mix(n01, n11, t.x), t.y);
}

// A simple 3D hash function (returns pseudo-random vec3 between 0 and 1)
vec3 hash33(vec3 p) {
    p = fract(p * vec3(443.897, 441.423, 437.195));
    p += dot(p, p.yxz + 19.19);
    return fract((p.xxy + p.yxx) * p.zyx);
}

float starLayer(vec3 dir) {
    // 1. Tiling: Scale the direction to create a grid
    float scale = 100.0;
    vec3 id = floor(dir * scale);
    vec3 local_uv = fract(dir * scale); // 0.0 to 1.0 inside the cell

    // 2. Random Position: Where is the star in this cell?
    vec3 star_pos = hash33(id);

    // 3. Animation: Twinkle logic using the 'time' uniform from your Lighting block
    // We use the star's unique ID to give it a unique offset so they don't pulse in sync
    float brightness = 0.5 + 0.5 * sin(time * 3.0 + star_pos.x * 100.0);

    // 4. Distance check: Are we looking at the star?
    // Centering the star in the cell (0.5) + jitter (star_pos - 0.5)
    vec3 center = vec3(0.5) + (star_pos - 0.5) * 0.8;
    float dist = length(local_uv - center);

    // 5. Draw: Sharp circle
    float radius = 0.05 * brightness; // Modulate radius by brightness for "glow"
    return smoothstep(radius, radius * 0.5, dist);
}

// Basic 3D Value Noise (Reuse your logic but adaptable to 3D input)
float noise3D(vec3 p) {
    // ... (Your bilinear interpolation logic extended to trilinear) ...
    // Note: For best results, research "Gradient Noise" or "Simplex Noise"
    // as they have fewer artifacts than the Value Noise in your file.
    return 0.5; // Placeholder
}

// Fractal Brownian Motion
// float fbm(vec3 p) {
//     float value = 0.0;
//     float amplitude = 0.5;
//     for (int i = 0; i < 5; i++) {
//         value += amplitude * noise3D(p);
//         p *= 2.0; // Higher frequency
//         amplitude *= 0.5; // Lower impact
//     }
//     return value;
// }
float fbm(vec2 p) {
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 4; i++) {
        value += amplitude * snoise(p); // Use your existing noise function
        p *= 2.0;
        amplitude *= 0.5;
    }
    return value;
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
    color = mix(color, color*0.6, snoise(world_ray.xy * 10.0));

    float top_mix = smoothstep(0.1, 0.6, y);
    vec3 final_color = mix(color, top_sky_color, top_mix);

    // --- 2. Nebula/Haze Layer (Domain Warping + FBM) ---
    // Use the world_ray.xz component to map the noise to the sphere
    vec2 p = world_ray.xz * 4.0;

    // Domain Warping: Offset coordinates by time-animated noise
    vec2 warp_offset = vec2(fbm(p + time * 0.05), fbm(p.yx + time * 0.05));

    // Final Nebula Noise calculation
    float nebula_noise = fbm(p + warp_offset * 0.5);

    // Map noise to a color palette (e.g., magenta and cyan for cosmic clouds)
    vec3 nebula_palette = mix(vec3(0.0, 0.1, 0.4), vec3(0.8, 0.2, 0.7), nebula_noise);

    // Blend the nebula color into the base sky, mostly visible in the dark top_sky area
    float nebula_strength = smoothstep(0.2, 0.6, top_mix); // Only fade it in when sky is dark
    final_color = mix(final_color, nebula_palette * 1.5, nebula_strength * 0.4);

    // --- 3. Star Field Layer (Additive Blend) ---
    // Add the twinkling stars on top
    float stars = starLayer(world_ray);

    // Stars should be white/blue and use an additive blend for a bright sparkle
    final_color += stars * vec3(1.0, 0.9, 0.8);

    // --- Final Output ---
    FragColor = vec4(final_color, 1.0);
}
