#ifndef HELPERS_LIGHTING_SIMPLE_GLSL
#define HELPERS_LIGHTING_SIMPLE_GLSL

// Simple lighting without shadow support
// Use this for shaders that don't need shadows (sky, trails, etc.)
// DEPRECATED: Include helpers/lighting.glsl and call apply_lighting_no_shadows instead.
//
// This file now simply includes the main lighting.glsl which contains both
// shadowed and shadow-free versions of all lighting functions:
//   - apply_lighting()                      -> with shadows
//   - apply_lighting_no_shadows()           -> without shadows
//   - apply_lighting_pbr()                  -> PBR with shadows
//   - apply_lighting_pbr_no_shadows()       -> PBR without shadows
//   - apply_lighting_pbr_iridescent_no_shadows() -> iridescent effect

#include "lighting.glsl"

#endif // HELPERS_LIGHTING_SIMPLE_GLSL
