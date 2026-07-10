#version 460 core
layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

in vec3 vColor[];
in float vIntensity[];
in vec3 vViewPos[];

out vec3 fColor;
out float fIntensity;
out vec2 fTexCoord;

uniform mat4 projection;
uniform float thickness = 1.5;

void main() {
    vec3 p0 = vViewPos[0];
    vec3 p1 = vViewPos[1];

    vec3 dir = p1 - p0;
    float len = length(dir);
    if (len > 0.0001) {
        dir /= len;
    } else {
        dir = vec3(0.0, 1.0, 0.0);
    }

    // In view space, the camera is at (0,0,0)
    // The vector from the camera to the segment can be approximated by p0
    vec3 viewDir = normalize(p0);
    vec3 right = cross(dir, viewDir);
    float rightLen = length(right);
    if (rightLen > 0.0001) {
        right /= rightLen;
    } else {
        right = cross(dir, vec3(1.0, 0.0, 0.0));
        float rLen = length(right);
        if (rLen > 0.0001) {
            right /= rLen;
        } else {
            right = vec3(0.0, 0.0, 1.0);
        }
    }

    float half_thick = thickness * 0.5;

    // Vertex 0: P0, Left
    vec3 pos0_L = p0 - right * half_thick;
    fColor = vColor[0];
    fIntensity = vIntensity[0];
    fTexCoord = vec2(-1.0, 0.0);
    gl_Position = projection * vec4(pos0_L, 1.0);
    EmitVertex();

    // Vertex 1: P0, Right
    vec3 pos0_R = p0 + right * half_thick;
    fColor = vColor[0];
    fIntensity = vIntensity[0];
    fTexCoord = vec2(1.0, 0.0);
    gl_Position = projection * vec4(pos0_R, 1.0);
    EmitVertex();

    // Vertex 2: P1, Left
    vec3 pos1_L = p1 - right * half_thick;
    fColor = vColor[1];
    fIntensity = vIntensity[1];
    fTexCoord = vec2(-1.0, 1.0);
    gl_Position = projection * vec4(pos1_L, 1.0);
    EmitVertex();

    // Vertex 3: P1, Right
    vec3 pos1_R = p1 + right * half_thick;
    fColor = vColor[1];
    fIntensity = vIntensity[1];
    fTexCoord = vec2(1.0, 1.0);
    gl_Position = projection * vec4(pos1_R, 1.0);
    EmitVertex();

    EndPrimitive();
}
