#version 330 core
out vec4 FragColor;

in vec3 WorldPos;
in vec3 Normal;

uniform vec3 viewPos;
uniform vec3 lightPos;
uniform vec3 lightColor;

void main()
{
    // Ambient
    float ambientStrength = 0.05;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - WorldPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular
    float specularStrength = 0.3;
    vec3 viewDir = normalize(viewPos - WorldPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;

    // Final color
    vec3 surfaceColor = vec3(0.05, 0.05, 0.08); // Dark granite color
    vec3 result = (ambient + diffuse + specular) * surfaceColor;
    FragColor = vec4(result, 1.0);
}
