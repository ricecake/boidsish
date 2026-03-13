#include <iostream>
#include <glm/glm.hpp>

struct Light {
    glm::vec3 position;
    float intensity;
    glm::vec3 color;
    int type;
    glm::vec3 direction;
    float inner_cutoff;
    float outer_cutoff;
    float padding[3];
};

struct Lighting {
    Light lights[10];
    int num_lights;
    float worldScale;
    float dayTime;
    float nightFactor;
    alignas(16) glm::vec3 viewPos;
    alignas(16) glm::vec3 ambient_light;
    float time;
    alignas(16) glm::vec3 viewDir;
};

int main() {
    std::cout << "Size of Light: " << sizeof(Light) << std::endl;
    std::cout << "Size of Lighting: " << sizeof(Lighting) << std::endl;
    std::cout << "Offset of viewPos: " << offsetof(Lighting, viewPos) << std::endl;
    std::cout << "Offset of ambient_light: " << offsetof(Lighting, ambient_light) << std::endl;
    std::cout << "Offset of time: " << offsetof(Lighting, time) << std::endl;
    std::cout << "Offset of viewDir: " << offsetof(Lighting, viewDir) << std::endl;
    return 0;
}
