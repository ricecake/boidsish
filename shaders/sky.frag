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


//
// Description : Array and textureless GLSL 2D/3D/4D simplex
//               noise functions.
//      Author : Ian McEwan, Ashima Arts.
//  Maintainer : ijm
//     Lastmod : 20110822 (ijm)
//     License : Copyright (C) 2011 Ashima Arts. All rights reserved.
//               Distributed under the MIT License. See LICENSE file.
//               https://github.com/ashima/webgl-noise
//

vec3 mod289(vec3 x) {
  return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 mod289(vec4 x) {
  return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 permute(vec4 x) {
     return mod289(((x*34.0)+1.0)*x);
}

vec4 taylorInvSqrt(vec4 r)
{
  return 1.79284291400159 - 0.85373472095314 * r;
}

float snoise(vec3 v)
  {
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

  //   x0 = x0 - 0.0 + 0.0 * C.xxx;
  //   x1 = x0 - i1  + 1.0 * C.xxx;
  //   x2 = x0 - i2  + 2.0 * C.xxx;
  //   x3 = x0 - 1.0 + 3.0 * C.xxx;
  vec3 x1 = x0 - i1 + C.xxx;
  vec3 x2 = x0 - i2 + C.yyy; // 2.0*C.x = 1/3 = C.y
  vec3 x3 = x0 - D.yyy;      // -1.0+3.0*C.x = -0.5 = -D.y

// Permutations
  i = mod289(i);
  vec4 p = permute( permute( permute(
             i.z + vec4(0.0, i1.z, i2.z, 1.0 ))
           + i.y + vec4(0.0, i1.y, i2.y, 1.0 ))
           + i.x + vec4(0.0, i1.x, i2.x, 1.0 ));

// Gradients: 7x7 points over a square, mapped onto an octahedron.
// The ring size 17*17 = 289 is close to a multiple of 49 (49*6 = 294)
  float n_ = 0.142857142857; // 1.0/7.0
  vec3  ns = n_ * D.wyz - D.xzx;

  vec4 j = p - 49.0 * floor(p * ns.z * ns.z);  //  mod(p,7*7)

  vec4 x_ = floor(j * ns.z);
  vec4 y_ = floor(j - 7.0 * x_ );    // mod(j,N)

  vec4 x = x_ *ns.x + ns.yyyy;
  vec4 y = y_ *ns.x + ns.yyyy;
  vec4 h = 1.0 - abs(x) - abs(y);

  vec4 b0 = vec4( x.xy, y.xy );
  vec4 b1 = vec4( x.zw, y.zw );

  //vec4 s0 = vec4(lessThan(b0,0.0))*2.0 - 1.0;
  //vec4 s1 = vec4(lessThan(b1,0.0))*2.0 - 1.0;
  vec4 s0 = floor(b0)*2.0 + 1.0;
  vec4 s1 = floor(b1)*2.0 + 1.0;
  vec4 sh = -step(h, vec4(0.0));

  vec4 a0 = b0.xzyw + s0.xzyw*sh.xxyy ;
  vec4 a1 = b1.xzyw + s1.xzyw*sh.zzww ;

  vec3 p0 = vec3(a0.xy,h.x);
  vec3 p1 = vec3(a0.zw,h.y);
  vec3 p2 = vec3(a1.xy,h.z);
  vec3 p3 = vec3(a1.zw,h.w);

//Normalise gradients
  vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
  p0 *= norm.x;
  p1 *= norm.y;
  p2 *= norm.z;
  p3 *= norm.w;

// Mix final noise value
  vec4 m = max(0.6 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
  m = m * m;
  return 42.0 * dot( m*m, vec4( dot(p0,x0), dot(p1,x1),
                                dot(p2,x2), dot(p3,x3) ) );
  }

// sky.frag


// A simple 3D hash function (returns pseudo-random vec3 between 0 and 1)
vec3 hash33(vec3 p) {
    p = fract(p * vec3(443.897, 441.423, 437.195));
    p += dot(p, p.yxz + 19.19);
    return fract((p.xxy + p.yxx) * p.zyx);
}
// A robust 1D hash function (Returns float between 0.0 and 1.0)
float hash11(float p) {
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p = fract(p);
    return fract(p * 0.5);
}

// Function to convert 3D integer coordinates to a unique float seed
float getSeed(vec3 p) {
    // We only need the integer part of the ID for a unique seed
    return p.x * 123.0 + p.y * 456.0 + p.z * 789.0;
}

float starLayer(vec3 dir) {
    // 1. Tiling: Scale the direction to create a grid
    float scale = 100.0;
    vec3 id = floor(dir * scale);
    vec3 local_uv = fract(dir * scale); // 0.0 to 1.0 inside the cell

    // 2. Random Position: Where is the star in this cell?
    vec3 star_pos = hash33(id);
//     float seed = getSeed(id);
// vec3 star_pos = vec3(
//         hash11(seed),
//         hash11(seed + 1.0), // Offset the seed for the Y component
//         hash11(seed + 2.0)  // Offset the seed for the Z component
//     );

    // 3. Animation: Twinkle logic using the 'time' uniform from your Lighting block
    // We use the star's unique ID to give it a unique offset so they don't pulse in sync
    float brightness = abs(sin(time/2 + star_pos.x*100));

    // 4. Distance check: Are we looking at the star?
    // Centering the star in the cell (0.5) + jitter (star_pos - 0.5)
    vec3 center = vec3(0.5) + (star_pos - 0.5) * 0.8;
    float dist = length(local_uv - center);

    // 5. Draw: Sharp circle
    float radius = 0.05 * brightness; // Modulate radius by brightness for "glow"
    return smoothstep(radius, radius * 0.5, dist);
}

float fbm(vec3 p) { // Now accepts 3D vector
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 4; i++) {
        value += amplitude * snoise(p);
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
    color = mix(color, color*0.6, snoise(world_ray * 10.0));

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

    // final_color = vec3(0.0); // Start with black to clearly see the stars
    // Stars should be white/blue and use an additive blend for a bright sparkle
    final_color += stars * vec3(1.0, 0.9, 0.8);

    // --- Final Output ---
    FragColor = vec4(final_color, 1.0);
}
