#version 420 core

layout(triangles, equal_spacing, cw) in;

in vec3 aPos_tc[];
in vec3 aNormal_tc[];
in vec2 aTexCoords_tc[];

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;
out vec3 barycentric;
out vec3 vs_color;

#include "visual_effects.glsl"
#include "visual_effects.vert"

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec4 clipPlane;
uniform float ripple_strength;
uniform bool isColossal = true;

layout(std140) uniform Lighting {
    vec3 lightPos;
    vec3 viewPos;
    vec3 lightColor;
    float time;
};

void main()
{
    // Interpolate vertex attributes
    vec3 p0 = gl_TessCoord.x * aPos_tc[0];
    vec3 p1 = gl_TessCoord.y * aPos_tc[1];
    vec3 p2 = gl_TessCoord.z * aPos_tc[2];
    vec3 aPos = p0 + p1 + p2;

    vec3 n0 = gl_TessCoord.x * aNormal_tc[0];
    vec3 n1 = gl_TessCoord.y * aNormal_tc[1];
    vec3 n2 = gl_TessCoord.z * aNormal_tc[2];
    vec3 aNormal = normalize(n0 + n1 + n2);

    vec2 t0 = gl_TessCoord.x * aTexCoords_tc[0];
    vec2 t1 = gl_TessCoord.y * aTexCoords_tc[1];
    vec2 t2 = gl_TessCoord.z * aTexCoords_tc[2];
    vec2 aTexCoords = t0 + t1 + t2;

    // Transformation logic from vis.vert
    vec3 displacedPos = aPos;
    vec3 displacedNormal = aNormal;

    if (glitched_enabled == 1) {
        displacedPos = applyGlitch(displacedPos, time);
    }

    if (ripple_strength > 0.0) {
        float frequency = 20.0;
        float speed = 3.0;
        float amplitude = ripple_strength;

        float wave = sin(frequency * (aPos.x + aPos.z) + time * speed);
        displacedPos = aPos + aNormal * wave * amplitude;

        vec3 gradient = vec3(
            cos(frequency * (aPos.x + aPos.z) + time * speed) * frequency * amplitude,
            0.0,
            cos(frequency * (aPos.x + aPos.z) + time * speed) * frequency * amplitude
        );
        displacedNormal = normalize(aNormal - gradient);
    }

    FragPos = vec3(model * vec4(displacedPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * displacedNormal;
    TexCoords = aTexCoords;

    if (wireframe_enabled == 1) {
        // This is now handled in the geometry shader
    }

    if (isColossal) {
        mat4 staticView = mat4(mat3(view));
        vec3 skyPositionOffset = vec3(0.0, -10.0, -500.0);
        vec4 world_pos = model * vec4(displacedPos * 50, 1.0);
        world_pos.xyz += skyPositionOffset;
        gl_Position = projection * staticView * world_pos;
        gl_Position = gl_Position.xyww;
        FragPos = world_pos.xyz;
    } else {
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }

    gl_ClipDistance[0] = dot(vec4(FragPos, 1.0), clipPlane);
}
