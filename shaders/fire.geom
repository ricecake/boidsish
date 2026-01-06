#version 430 core

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

// From Vertex Shader
in vec3 v_pos[];
in vec3 v_vel[];
in float v_lifetime[];
in float v_point_size[];
flat in int v_style[];

// To Fragment Shader
out float f_lifetime;
out vec2 f_uv;
flat out int f_style;

uniform mat4 u_view;
uniform mat4 u_projection;
uniform vec3 u_camera_pos;

void main() {
    if (v_lifetime[0] <= 0.0) {
        return;
    }

    // Pass through attributes for the primitive
    f_lifetime = v_lifetime[0];
    f_style = v_style[0];

    if (v_style[0] == 3) { // Tracer
        vec3 pos = v_pos[0];
        vec3 vel_dir = normalize(v_vel[0]);
        vec3 view_dir = normalize(u_camera_pos - pos);

        // Foreshortening and stretching logic
        float foreshortening = 1.0 - abs(dot(vel_dir, view_dir));
        float speed_stretch = 1.0 + length(v_vel[0]) * 0.05;
        float length = speed_stretch * foreshortening * 0.5; // half length
        float width = 0.05; // half width

        vec3 up = normalize(cross(vel_dir, view_dir));
        vec3 right = vel_dir;

        // Generate a camera-aligned quad oriented along the velocity vector
        gl_Position = u_projection * u_view * vec4(pos - right * length - up * width, 1.0);
        f_uv = vec2(0.0, 0.0);
        EmitVertex();

        gl_Position = u_projection * u_view * vec4(pos + right * length - up * width, 1.0);
        f_uv = vec2(1.0, 0.0);
        EmitVertex();

        gl_Position = u_projection * u_view * vec4(pos - right * length + up * width, 1.0);
        f_uv = vec2(0.0, 1.0);
        EmitVertex();

        gl_Position = u_projection * u_view * vec4(pos + right * length + up * width, 1.0);
        f_uv = vec2(1.0, 1.0);
        EmitVertex();

    } else { // Other particles (standard billboard)
        vec3 pos = v_pos[0];
        float size = v_point_size[0] * 0.02;

        vec3 camera_right = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
        vec3 camera_up = vec3(u_view[0][1], u_view[1][1], u_view[2][1]);

        gl_Position = u_projection * u_view * vec4(pos - camera_right * size - camera_up * size, 1.0);
        f_uv = vec2(0.0, 0.0);
        EmitVertex();

        gl_Position = u_projection * u_view * vec4(pos + camera_right * size - camera_up * size, 1.0);
        f_uv = vec2(1.0, 0.0);
        EmitVertex();

        gl_Position = u_projection * u_view * vec4(pos - camera_right * size + camera_up * size, 1.0);
        f_uv = vec2(0.0, 1.0);
        EmitVertex();

        gl_Position = u_projection * u_view * vec4(pos + camera_right * size + camera_up * size, 1.0);
        f_uv = vec2(1.0, 1.0);
        EmitVertex();
    }

    EndPrimitive();
}
