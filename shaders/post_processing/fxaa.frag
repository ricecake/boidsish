#version 460 core

uniform sampler2D screenTexture;
uniform vec2      RESOLUTION;
uniform float     uReduceMin;
uniform float     uReduceMul;
uniform float     uSpanMax;

#define SAMPLEFXAA_REDUCE_MIN uReduceMin
#define SAMPLEFXAA_REDUCE_MUL uReduceMul
#define SAMPLEFXAA_SPAN_MAX   uSpanMax

#include "lygia/sample/fxaa.glsl"

in vec2 TexCoords;
out vec4 FragColor;

void main() {
    FragColor = sampleFXAA(screenTexture, TexCoords, 1.0 / RESOLUTION);
}
