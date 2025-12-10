#version 330 core
out vec4 FragColor;

in vec3 color;
in float fade;
in vec3 normal;
in vec3 frag_pos;

layout (std140) uniform Lighting {
    vec3 lightPos;
    vec3 viewPos;
    vec3 lightColor;
};

void main()
{
    // Ambient
    float ambient_strength = 0.2;
    vec3 ambient = ambient_strength * lightColor;

    // Diffuse
    vec3 norm = normalize(normal);
    vec3 light_dir = normalize(lightPos - frag_pos);
    float diff = max(dot(norm, light_dir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular
    float specular_strength = 1.0;
    vec3 view_dir = normalize(viewPos - frag_pos);
    vec3 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 64);
    vec3 specular = specular_strength * spec * lightColor;

    vec3 result = (ambient + diffuse) * color + specular;
    FragColor = vec4(result, 1);
}
