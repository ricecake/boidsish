#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor; // For non-instanced vertex colors
layout (location = 3) in vec3 instancePosition;
layout (location = 4) in float instanceSize;
layout (location = 5) in vec4 instanceColor;

out vec3 FragPos;
out vec3 Normal;
out vec3 Color;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform bool useVertexColor;
uniform bool useInstancedDrawing;
uniform vec4 clipPlane;

void main()
{
    if (useInstancedDrawing) {
        mat4 instanceModel = mat4(
            vec4(instanceSize, 0, 0, 0),
            vec4(0, instanceSize, 0, 0),
            vec4(0, 0, instanceSize, 0),
            vec4(instancePosition, 1.0)
        );

        FragPos = vec3(instanceModel * vec4(aPos, 1.0));
        Normal = mat3(transpose(inverse(instanceModel))) * aNormal;
        Color = instanceColor.rgb;
        gl_Position = projection * view * vec4(FragPos, 1.0);
    } else {
        FragPos = vec3(model * vec4(aPos, 1.0));
        Normal = mat3(transpose(inverse(model))) * aNormal;
        if(useVertexColor) {
            Color = aColor;
        }
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
    gl_ClipDistance[0] = dot(vec4(FragPos, 1.0), clipPlane);
}
