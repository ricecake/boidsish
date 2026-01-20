#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

struct Light {
    vec3 position;
    float intensity;
    vec3 color;
    float padding; // for std140 alignment
};

const int MAX_LIGHTS = 10;

layout(std140) uniform Lighting {
    Light lights[MAX_LIGHTS];
    int num_lights;
    vec3 viewPos;
    float time;
};

#endif
