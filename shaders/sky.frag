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


// vec4 permute(vec4 x){return mod(((x*34.0)+1.0)*x, 289.0);}
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

  vec3 x1 = x0 - i1 + C.xxx;
  vec3 x2 = x0 - i2 + C.yyy;
  vec3 x3 = x0 - D.yyy;

// Permutations
  i = mod289(i);
  vec4 p = permute( permute( permute(
             i.z + vec4(0.0, i1.z, i2.z, 1.0 ))
           + i.y + vec4(0.0, i1.y, i2.y, 1.0 ))
           + i.x + vec4(0.0, i1.x, i2.x, 1.0 ));

  float n_ = 0.142857142857;
  vec3  ns = n_ * D.wyz - D.xzx;

  vec4 j = p - 49.0 * floor(p * ns.z * ns.z);

  vec4 x_ = floor(j * ns.z);
  vec4 y_ = floor(j - 7.0 * x_ );

  vec4 x = x_ *ns.x + ns.yyyy;
  vec4 y = y_ *ns.x + ns.yyyy;
  vec4 h = 1.0 - abs(x) - abs(y);

  vec4 b0 = vec4( x.xy, y.xy );
  vec4 b1 = vec4( x.zw, y.zw );

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
    return p.x * 123.0 + p.y * 456.0 + p.z * 789.0;
}

float starLayer(vec3 dir) {
    // 1. Tiling: Scale the direction to create a grid
    float scale = 100.0;
    vec3 id = floor(dir * scale);
    vec3 local_uv = fract(dir * scale); // 0.0 to 1.0 inside the cell

    // 2. Random Position: Where is the star in this cell?
    vec3 star_pos = hash33(id);

    // 3. Animation: Twinkle logic
    // use the star's unique ID to give it a unique offset so they don't pulse in sync
    float brightness = abs(sin(time/2 + star_pos.x*100));

    // 4. Distance check: Are we looking at the star?
    // Centering the star in the cell (0.5) + jitter (star_pos - 0.5)
    vec3 center = vec3(0.5) + (star_pos - 0.5) * 0.8;
    float dist = length(local_uv - center);

    float radius = 0.05 * brightness; // Modulate radius by brightness for "glow"
    return smoothstep(radius, radius * 0.5, dist);
}

float fbm(vec3 p) {
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
    color = mix(color, color*0.9, snoise(world_ray * 10.0));

    float top_mix = smoothstep(0.1, 0.6, y);
    vec3 final_color = mix(color, top_sky_color, top_mix);

    // --- 2. Nebula/Haze Layer (Domain Warping + FBM) ---
    vec3 p = world_ray * 4.0;
    vec3 warp_offset = vec3(fbm(p + time * 0.05));
    float nebula_noise = fbm(p + warp_offset * 0.5);

    // Map noise to a color palette (e.g., magenta and cyan for cosmic clouds)
    vec3 nebula_palette = mix(vec3(0.0, 0.1, 0.4), vec3(0.8, 0.2, 0.7), nebula_noise);

    // Blend the nebula color into the base sky, mostly visible in the dark top_sky area
    float nebula_strength = smoothstep(0.2, 0.6, top_mix); // Only fade it in when sky is dark
    final_color = mix(final_color, nebula_palette * 1.5, nebula_strength * 0.4);

    // --- 3. Star Field Layer (Additive Blend) ---
    float stars = starLayer(world_ray);
    final_color += stars * vec3(1.0, 0.9, 0.8);
    FragColor = vec4(final_color, 1.0);
}
