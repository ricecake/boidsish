#version 420 core

layout (triangles) in;
// Max vertices: 4 for triangle wireframe (A-B-C-A) + 6 for 3 normals (2 verts each) = 10
layout (line_strip, max_vertices = 10) out;

// Inputs must match the Outputs of your TES (terrain.tes)
in vec3 FragPos[];
in vec3 Normal[];

uniform mat4 view;
uniform mat4 projection;
uniform float uNormalLength = 0.01; // Adjust this to see normals better

void main() {
    mat4 pv = projection * view;

    // --- PHASE 1: Render the Mesh (Wireframe) ---
    // We loop 0 -> 1 -> 2 -> 0 to close the triangle loop
    for(int i = 0; i < 3; ++i) {
        // gl_in[i].gl_Position is ALREADY projected in the TES.
        // We just pass it through. Do not multiply by pv again!
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
    }
    // Close the loop (back to 0)
    gl_Position = gl_in[0].gl_Position;
    EmitVertex();

    EndPrimitive(); // Finish the triangle loop

    // --- PHASE 2: Render the Normals (Hairs) ---
    for(int i = 0; i < 3; ++i) {
        // 1. Start of the hair (on the surface)
        // We use gl_in because it is already projected correctly.
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();

        // 2. End of the hair (sticking out)
        // We must calculate this manually in World Space using FragPos
        vec3 normalTip = FragPos[i] + (Normal[i] * uNormalLength);

        // NOW we project the tip, because it wasn't calculated in TES
        gl_Position = pv * vec4(normalTip, 1.0);
        EmitVertex();

        EndPrimitive(); // Break the line so it doesn't connect to the next hair
    }
}