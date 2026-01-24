#version 430 core

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

uniform mat4 u_projection;
uniform vec3 u_camera_pos; // We'll need the camera's position for billboarding

// Input from the vertex shader
in float v_lifetime[];
in vec4  view_pos[];
flat in int   v_style[];

// Output to the fragment shader
out float    f_lifetime;
out vec2     f_tex_coord;
flat out int f_style;

void main() {
    if (v_style[0] < 0) {
        // Don't emit geometry for dead particles
        return;
    }

    // Determine particle size based on style and lifetime
    float size = 1.0;
    if (v_style[0] == 0) { // MissileExhaust
        size = smoothstep(1.0 - v_lifetime[0], v_lifetime[0], v_lifetime[0] / 2.0) * 0.5;
    } else if (v_style[0] == 1) { // Explosion
        size = (1.0 - (1.0 - v_lifetime[0]) * (1.0 - v_lifetime[0])) * 1.0;
    } else { // Fire
        size = smoothstep(2.0 * (1.0 - v_lifetime[0]), v_lifetime[0], v_lifetime[0] / 2.5) * 0.75;
    }

    // Get the particle's position in view space from the vertex shader
    vec3 pos = view_pos[0].xyz;

    // Billboard the quad to face the camera.
    // In view space, the camera is at the origin (0,0,0), so we don't need
    // the camera position. We construct the quad in the XY plane.
    vec3 up = vec3(0, 1, 0) * size;
    vec3 right = vec3(1, 0, 0) * size;

    f_style = v_style[0];

    // Bottom-left
    gl_Position = u_projection * vec4(pos - right - up, 1.0);
    f_tex_coord = vec2(0.0, 0.0);
    f_lifetime = v_lifetime[0];
    EmitVertex();

    // Top-left
    gl_Position = u_projection * vec4(pos - right + up, 1.0);
    f_tex_coord = vec2(0.0, 1.0);
    f_lifetime = v_lifetime[0];
    EmitVertex();

    // Bottom-right
    gl_Position = u_projection * vec4(pos + right - up, 1.0);
    f_tex_coord = vec2(1.0, 0.0);
    f_lifetime = v_lifetime[0];
    EmitVertex();

    // Top-right
    gl_Position = u_projection * vec4(pos + right + up, 1.0);
    f_tex_coord = vec2(1.0, 1.0);
    f_lifetime = v_lifetime[0];
    EmitVertex();

    EndPrimitive();
}
