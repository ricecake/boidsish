#version 430 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;
out mat4 vModel;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    vModel = model;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
