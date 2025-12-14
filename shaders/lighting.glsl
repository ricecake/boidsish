#ifndef LIGHTING_UBO_H
#define LIGHTING_UBO_H

layout (std140) uniform Lighting {
    vec3 lightPos;
    vec3 viewPos;
    vec3 lightColor;
    float time;
};

#endif
