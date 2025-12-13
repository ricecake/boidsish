#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;

layout (std140) uniform Lighting {
    vec3 lightPos;
    vec3 viewPos;
    vec3 lightColor;
};

void main()
{
    vec3 objectColor = vec3(0.1, 0.2, 0.5); // A deep blue

    // Ambient
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular
    float specularStrength = 0.8;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64);
    vec3 specular = specularStrength * spec * lightColor;

    vec3 result = (ambient + diffuse) * objectColor + specular; // Add specular on top
    FragColor = vec4(result, 1.0);
}
