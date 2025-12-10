#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec3 Color;
in float Progress;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform int useVertexColor;

void main()
{
    vec3 color = Color;
    if (useVertexColor == 0) {
        color = vec3(1.0, 0.0, 0.0);
    }

    // Ambient
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;

    vec3 result = (ambient + diffuse) * color + specular;
    float alpha = Progress;
    FragColor = vec4(result, alpha);
}
