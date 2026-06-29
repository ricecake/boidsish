#version 460 core

uniform sampler2D screenTexture;
uniform vec2      RESOLUTION;
uniform float     uReduceMin;
uniform float     uReduceMul;
uniform float     uSpanMax;

in vec2 TexCoords;
out vec4 FragColor;

// Fixed FXAA implementation to avoid "black sky" issue found in some library versions
// and to ensure correct coordinate scaling.
vec4 sampleFXAA_fixed(sampler2D tex, vec2 uv, vec2 pixel) {
    vec3 rgbNW  = texture(tex, uv + vec2(-1.0, -1.0) * pixel).xyz;
    vec3 rgbNE  = texture(tex, uv + vec2( 1.0, -1.0) * pixel).xyz;
    vec3 rgbSW  = texture(tex, uv + vec2(-1.0,  1.0) * pixel).xyz;
    vec3 rgbSE  = texture(tex, uv + vec2( 1.0,  1.0) * pixel).xyz;
    vec4 rgbaM  = texture(tex, uv);
    vec3 rgbM   = rgbaM.xyz;

    vec3 luma   = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * uReduceMul), uReduceMin);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

    dir = min(vec2(uSpanMax, uSpanMax),
              max(vec2(-uSpanMax, -uSpanMax),
                  dir * rcpDirMin)) * pixel;

    vec4 rgbA = 0.5 * (
        texture(tex, uv + dir * (1.0/3.0 - 0.5)) +
        texture(tex, uv + dir * (2.0/3.0 - 0.5)));
    vec4 rgbB = rgbA * 0.5 + 0.25 * (
        texture(tex, uv + dir * -0.5) +
        texture(tex, uv + dir * 0.5));

    float lumaB = dot(rgbB.rgb, luma);
    if ((lumaB < lumaMin) || (lumaB > lumaMax))
        return vec4(rgbA.rgb, rgbaM.a);
    else
        return vec4(rgbB.rgb, rgbaM.a);
}

void main() {
    FragColor = sampleFXAA_fixed(screenTexture, TexCoords, 1.0 / RESOLUTION);
}
