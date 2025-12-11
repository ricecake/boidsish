#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;

out vec3 vs_color;
out vec3 vs_normal;
out vec3 vs_frag_pos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec4 clipPlane;

void main()
{
    vs_color = aColor;
    vs_normal = mat3(transpose(inverse(model))) * aNormal;
    vs_frag_pos = vec3(model * vec4(aPos, 1.0));

    vec4 world_pos = model * vec4(aPos, 1.0);
    gl_ClipDistance[0] = dot(world_pos, clipPlane);
    gl_Position = projection * view * world_pos;
}
