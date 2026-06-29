#version 460 core

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;
uniform vec2      RESOLUTION;
uniform float     uFocusPoint;
uniform float     uFocusScale;
uniform float     uBlurSize;

#define SAMPLEDOF_BLUR_SIZE uBlurSize

#include "lygia/sample/dof.glsl"

in vec2 TexCoords;
out vec4 FragColor;

void main() {
    FragColor = vec4(sampleDoF(screenTexture, depthTexture, TexCoords, uFocusPoint, uFocusScale), 1.0);
}
