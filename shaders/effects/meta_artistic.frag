#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform float     time;

// Simplex 2D noise
// from https://gist.github.com/patriciogonzalezvivo/670c22f3966e662d2f83
vec3 permute(vec3 x) { return mod(((x*34.0)+1.0)*x, 289.0); }
float snoise(vec2 v){
  const vec4 C = vec4(0.211324865405187, 0.366025403784439, -0.577350269189626, 0.024390243902439);
  vec2 i  = floor(v + dot(v, C.yy) );
  vec2 x0 = v - i + dot(i, C.xx);
  vec2 i1;
  i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
  vec4 x12 = x0.xyxy + C.xxzz;
  x12.xy -= i1;
  i = mod(i, 289.0);
  vec3 p = permute( permute( i.y + vec3(0.0, i1.y, 1.0 )) + i.x + vec3(0.0, i1.x, 1.0 ));
  vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);
  m = m*m ;
  m = m*m ;
  vec3 x = 2.0 * fract(p * C.www) - 1.0;
  vec3 h = abs(x) - 0.5;
  vec3 ox = floor(x + 0.5);
  vec3 a0 = x - ox;
  m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );
  vec3 g;
  g.x  = a0.x  * x0.x  + h.x  * x0.y;
  g.yz = a0.yz * x12.xz + h.yz * x12.yw;
  return 130.0 * dot(m, g);
}

// FBM function
float fbm(vec2 x) {
    float v = 0.0;
    float a = 0.5;
    vec2 shift = vec2(100);
    mat2 rot = mat2(cos(0.5), sin(0.5), -sin(0.5), cos(0.50));
    for (int i = 0; i < 5; ++i) {
        v += a * snoise(x);
        x = rot * x * 2.0 + shift;
        a *= 0.5;
    }
    return v;
}

// Simple pseudo-random function
float random(vec2 st) {
	return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

vec4 applyGlitchEffect(vec2 uv) {
    // 1. Blocky Displacement
    float block_speed = 10.0;
    float block_size = 0.05;
    float block_intensity = 0.02;

    float line_noise = random(vec2(time * block_speed, floor(uv.y / block_size) * block_size));
    if (line_noise > 0.96) { // Affect only a small percentage of lines
        float displace = random(vec2(time * block_speed, uv.y)) * block_intensity;
        uv.x += displace;
    }

    // 2. Color Channel Separation (Chromatic Aberration)
    float separation_intensity = 0.01;
    float separation_speed = 20.0;

    float r_offset = (random(vec2(time * separation_speed, 1.0)) - 0.5) * 2.0 * separation_intensity;
    float g_offset = (random(vec2(time * separation_speed, 2.0)) - 0.5) * 2.0 * separation_intensity;
    float b_offset = (random(vec2(time * separation_speed, 3.0)) - 0.5) * 2.0 * separation_intensity;

    float r = texture(sceneTexture, uv + vec2(r_offset, 0.0)).r;
    float g = texture(sceneTexture, uv + vec2(g_offset, 0.0)).g;
    float b = texture(sceneTexture, uv + vec2(b_offset, 0.0)).b;

    return vec4(r, g, b, 1.0);
}

vec4 applyNegativeEffect(vec2 uv) {
    return vec4(vec3(1.0) - texture(sceneTexture, uv).rgb, 1.0);
}


void main() {
    vec2 uv = TexCoords;
    float noise = fbm(uv * 5.0 + time * 0.1);

    int effectIndex = int(mod(floor(abs(noise) * 10.0), 3.0));

    if (effectIndex == 0) {
        FragColor = texture(sceneTexture, uv);
    } else if (effectIndex == 1) {
        FragColor = applyGlitchEffect(uv);
    } else {
        FragColor = applyNegativeEffect(uv);
    }
}
