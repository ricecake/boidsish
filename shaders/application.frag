#version 420 core

out vec4 FragColor;

//- Inputs from vertex shader
in vec3 vs_FragPos;
in vec3 vs_Normal;
in vec3 vs_color;
in float vs_progress;
in vec3 vs_barycentric;
in vec3 vs_WorldPos;
in vec4 vs_ReflectionClipSpacePos;
in vec2 vs_TexCoords;

//- Uniforms for textures and settings
uniform sampler2D reflectionTexture;
uniform vec3 objectColor;
uniform int  useVertexColor;
uniform bool colorShift;

//- Uniforms to select the rendering path
uniform bool isVis;
uniform bool isPlane;
uniform bool isTrail;
uniform bool isSky;
uniform bool isTerrain;

//- Lighting and artistic effects
#include "helpers/lighting.glsl"
#include "artistic_effects.glsl"
#include "artistic_effects.frag"

//- Include modular logic
#include "modules/vis.frag.glsl"
#include "modules/plane.frag.glsl"
#include "modules/trail.frag.glsl"
#include "modules/sky.frag.glsl"
#include "modules/terrain.frag.glsl"

void main() {
    if (isVis) {
        fragment_vis();
    } else if (isPlane) {
        fragment_plane();
    } else if (isTrail) {
        fragment_trail();
    } else if (isSky) {
        fragment_sky();
    } else if (isTerrain) {
        fragment_terrain();
    }
}
