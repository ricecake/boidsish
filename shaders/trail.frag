#version 330 core
out vec4 FragColor;

in vec3 color;
in float fade;
in vec3 normal;
in vec3 frag_pos;

uniform vec3 view_pos;

void main()
{
    // Ambient
    float ambient_strength = 0.2;
    vec3 ambient = ambient_strength * vec3(1.0, 1.0, 1.0);

    // Diffuse
    vec3 light_pos = vec3(5.0, 10.0, 5.0);
    vec3 norm = normalize(normal);
    vec3 light_dir = normalize(light_pos - frag_pos);
    float diff = max(dot(norm, light_dir), 0.0);
    vec3 diffuse = diff * vec3(1.0, 1.0, 1.0);

    // Specular
    float specular_strength = 1.0;
    vec3 view_dir = normalize(view_pos - frag_pos);
    vec3 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 64);
    vec3 specular = specular_strength * spec * vec3(1.0, 1.0, 1.0);

    vec3 result = (ambient + diffuse) * color + specular;
    FragColor = vec4(result, 1);
}
