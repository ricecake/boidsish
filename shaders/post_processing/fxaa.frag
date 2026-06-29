#version 460 core

uniform sampler2D screenTexture;
uniform vec2      RESOLUTION;
uniform float     uReduceMin;
uniform float     uReduceMul;
uniform float     uSpanMax;
uniform float     uLumaThreshold;

in vec2 TexCoords;
out vec4 FragColor;

// HDR-safe luma calculation
float getLuma(vec3 rgb) {
    // Basic luma
    float l = dot(rgb, vec3(0.299, 0.587, 0.114));
    // Apply a simple tonemap to the luma for more stable edge detection in HDR
    return l / (1.0 + l);
}

vec3 safe_sample(vec2 uv) {
    vec3 c = texture(screenTexture, uv).rgb;
    // Replace NaNs and Infs with 0 to prevent black propagation
    if (any(isnan(c)) || any(isinf(c))) return vec3(0.0);
    return max(c, vec3(0.0));
}

void main() {
    vec2 pixel = 1.0 / RESOLUTION;

    vec4 colorM = texture(screenTexture, TexCoords);
    vec3 rgbM = colorM.rgb;
    if (any(isnan(rgbM)) || any(isinf(rgbM))) rgbM = vec3(0.0);
    rgbM = max(rgbM, vec3(0.0));

    float lumaM = getLuma(rgbM);

    // Fast exit for low luma areas to preserve sky/dark details
    if (lumaM < uLumaThreshold) {
        FragColor = vec4(rgbM, colorM.a);
        return;
    }

    vec3 rgbNW = safe_sample(TexCoords + vec2(-1.0, -1.0) * pixel);
    vec3 rgbNE = safe_sample(TexCoords + vec2( 1.0, -1.0) * pixel);
    vec3 rgbSW = safe_sample(TexCoords + vec2(-1.0,  1.0) * pixel);
    vec3 rgbSE = safe_sample(TexCoords + vec2( 1.0,  1.0) * pixel);

    float lumaNW = getLuma(rgbNW);
    float lumaNE = getLuma(rgbNE);
    float lumaSW = getLuma(rgbSW);
    float lumaSE = getLuma(rgbSE);

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

    vec3 rgbA = 0.5 * (
        safe_sample(TexCoords + dir * (1.0/3.0 - 0.5)) +
        safe_sample(TexCoords + dir * (2.0/3.0 - 0.5)));
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        safe_sample(TexCoords + dir * -0.5) +
        safe_sample(TexCoords + dir * 0.5));

    float lumaB = getLuma(rgbB);
    if ((lumaB < lumaMin) || (lumaB > lumaMax))
        FragColor = vec4(rgbA, colorM.a);
    else
        FragColor = vec4(rgbB, colorM.a);
}
