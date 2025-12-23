#version 420 core

//- Vertex attributes
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in float aProgress;
layout(location = 4) in vec2 aTexCoords;

//- Uniforms for matrices and settings
uniform mat4  model;
uniform mat4  view;
uniform mat4  projection;
uniform mat4  reflectionViewProjection;
uniform vec4  clipPlane;
uniform float ripple_strength;
uniform float base_thickness;
uniform float uPhongAlpha;

//- Uniforms to select the rendering path
uniform bool isVis;
uniform bool isPlane;
uniform bool isTrail;
uniform bool isSky;
uniform bool isTerrain;

//- Lighting and artistic effects
#include "helpers/lighting.glsl"
#include "artistic_effects.glsl"
#include "artistic_effects.vert"

//- Outputs to other shader stages
out vec3 vs_FragPos;
out vec3 vs_Normal;
out vec3 vs_color;
out float vs_progress;
out vec3 vs_barycentric;
out vec3 vs_WorldPos;
out vec4 vs_ReflectionClipSpacePos;
out vec2 vs_TexCoords;

//- Outputs for terrain tessellation
out vec3 vs_WorldPos_VS_out;
out vec2 vs_TexCoords_VS_out;
out vec3 vs_Normal_VS_out;
out vec3 vs_viewForward;

//- Include modular logic
#include "modules/vis.vert.glsl"
#include "modules/plane.vert.glsl"
#include "modules/trail.vert.glsl"
#include "modules/sky.vert.glsl"
#include "modules/terrain.vert.glsl"

void main() {
    if (isVis) {
        vertex_vis();
    } else if (isPlane) {
        vertex_plane();
    } else if (isTrail) {
        vertex_trail();
    } else if (isSky) {
        vertex_sky();
    } else if (isTerrain) {
        vertex_terrain();
    }
}
