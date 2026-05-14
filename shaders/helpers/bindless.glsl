#ifndef BINDLESS_GLSL
#define BINDLESS_GLSL

#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable

/**
 * @brief Helper macros for bindless texture access.
 */

// Use this to declare a bindless sampler uniform in a shader
#define BINDLESS_SAMPLER2D(name) layout(bindless_sampler) uniform sampler2D name

// If passing raw uint64 handles in a UBO or SSBO
#define BINDLESS_HANDLE uint64_t

#endif // BINDLESS_GLSL
