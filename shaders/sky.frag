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


// sky.frag

// --- 3D Simplex Noise (Full Replacement for old noise/snoise) ---
vec4 permute(vec4 x){return mod(((x*34.0)+1.0)*x, 289.0);}
vec4 taylorInvSqrt(vec4 r){return 1.79284291400159 - 0.85373472095314 * r;}

float snoise3D(vec3 v) {
  const vec2  C = vec2(1.0/6.0, 1.0/3.0) ;
  const vec4  D = vec4(0.0, 0.5, 1.0, 2.0);

  // First corner
  vec3 i  = floor(v + dot(v, C.yyy) );
  vec3 x0 =   v - i + dot(i, C.xxx) ;

  // Other corners
  vec3 g = step(x0.yzx, x0.xyz);
  vec3 l = 1.0 - g;
  vec3 i1 = min( g.xyz, l.zxy );
  vec3 i2 = max( g.xyz, l.zxy );

  //  x1 = x0 - i1 + 1.0 * C.xxx;
  //  x2 = x0 - i2 + 2.0 * C.xxx;
  //  x3 = x0 - 1.0 + 3.0 * C.xxx;
  vec3 x1 = x0 - i1 + C.xxx;
  vec3 x2 = x0 - i2 + C.yyy; // 2.0*C.x = 2/6 = 1/3 = C.y
  vec3 x3 = x0 - D.yyy;      // 1.0 - 3.0*C.x = 0.5 = D.y

  // Permutations
  i = mod(i, 289.0 );
  vec4 p = permute( permute( permute(
             i.z + vec4(0.0, i1.z, i2.z, 1.0 ))
           + i.y + vec4(0.0, i1.y, i2.y, 1.0 ))
           + i.x + vec4(0.0, i1.x, i2.x, 1.0 ));

  // Gradients: 7/8 of the cube faces
  vec3 m0 = max(0.6 - vec3(dot(x0,x0), dot(x1,x1), dot(x2,x2)), 0.0);
vec3 m1 = vec3(max(0.6 - dot(x3,x3), 0.0));
  m0 = m0 * m0;
  m1 = m1 * m1;

  // Gradients: 44 of them
  vec4 x = fract(p * (1.0 / 7.0)) * 2.0 - 1.0;
  vec4 h = abs(x) - 0.5;
  vec4 ox = floor(x + 0.5);
  vec4 a0 = x - ox;

vec4 inv_sqrt = taylorInvSqrt(vec4(dot(a0.xy,a0.xy), dot(a0.zw,a0.zw), dot(a0.xz,a0.xz), dot(a0.yw,a0.yw)));
m0 *= inv_sqrt.xyz;

// m1 (vec3) is scaled by the final factor (.w), broadcasted to vec3
m1 *= vec3(inv_sqrt.w);

// Use component swizzling (xyz, yzw) to match vec3 position vectors (x0-x3)
float n0 = dot(a0.xyz, x0);
float n1 = dot(a0.yzw, x1);
float n2 = dot(a0.yzw, x2);
float n3 = dot(a0.yzw, x3);
  return 32.0 * (m0.x * n0 + m0.y * n1 + m0.z * n2 + m1.x * n3);
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
// sky.frag

float fbm(vec3 p) { // Now accepts 3D vector
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 4; i++) {
        // *** Use the 3D Simplex function ***
        value += amplitude * snoise3D(p);
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
    color = mix(color, color*0.6, snoise3D(world_ray * 10.0));

    float top_mix = smoothstep(0.1, 0.6, y);
    vec3 final_color = mix(color, top_sky_color, top_mix);

    // --- 2. Nebula/Haze Layer (Domain Warping + FBM) ---
    // Use the world_ray.xz component to map the noise to the sphere
    vec3 p = world_ray * 4.0;

    // Domain Warping: Offset coordinates by time-animated noise
    vec3 warp_offset = vec3(fbm(p + time * 0.05));

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
