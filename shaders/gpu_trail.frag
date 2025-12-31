#version 430 core

// INPUT: Interpolated values from the Geometry Shader
in vec3 vs_frag_pos;
in vec3 vs_normal;
in vec3 vs_color;
in float vs_progress; // Represents the "age" of the trail segment

// OUTPUT: The final color of the fragment
out vec4 FragColor;

// Uniform Buffer Object for lighting information.
// This must match the layout defined in the C++ host code and other shaders.
layout (std140, binding = 0) uniform Lighting {
    vec3 lightPos;
    vec3 viewPos;
    vec3 lightColor;
};

void main() {
    // --- AMBIENT ---
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * lightColor;

    // --- DIFFUSE ---
    vec3 norm = normalize(vs_normal);
    vec3 lightDir = normalize(lightPos - vs_frag_pos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // --- SPECULAR ---
    float specularStrength = 0.8;
    vec3 viewDir = normalize(viewPos - vs_frag_pos);
    vec3 reflectDir = reflect(-lightDir, norm);
    // Use a tighter specular highlight for a "shinier" look
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64);
    vec3 specular = specularStrength * spec * lightColor;

    // Combine lighting components with the trail's vertex color
    vec3 result = (ambient + diffuse) * vs_color + specular;

    // Add a fade-out effect for the oldest parts of the trail
    float alpha = smoothstep(0.0, 0.2, vs_progress);

    FragColor = vec4(result, alpha);
}
