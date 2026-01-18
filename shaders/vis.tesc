#version 420 core

layout(vertices = 3) out;

in vec3 aPos_vs[];
in vec3 aNormal_vs[];
in vec2 aTexCoords_vs[];

out vec3 aPos_tc[];
out vec3 aNormal_tc[];
out vec2 aTexCoords_tc[];

void main()
{
    aPos_tc[gl_InvocationID] = aPos_vs[gl_InvocationID];
    aNormal_tc[gl_InvocationID] = aNormal_vs[gl_InvocationID];
    aTexCoords_tc[gl_InvocationID] = aTexCoords_vs[gl_InvocationID];

    if (gl_InvocationID == 0)
    {
        gl_TessLevelInner[0] = 1;
        gl_TessLevelOuter[0] = 1;
        gl_TessLevelOuter[1] = 1;
        gl_TessLevelOuter[2] = 1;
    }
}
