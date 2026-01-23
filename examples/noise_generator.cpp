#include <iostream>
#include <vector>
#include <cmath>
#include <glm/glm.hpp>
#include "Simplex.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

const int TEXTURE_WIDTH = 128;
const int TEXTURE_HEIGHT = 128;
const int TEXTURE_DEPTH = 128;

float tileableNoise(glm::vec3 p, float period) {
    p = glm::mod(p, glm::vec3(period));

    // Get the integer and fractional parts of the coordinates
    glm::vec3 p_int = floor(p);
    glm::vec3 p_fract = fract(p);

    // Calculate smooth blend weights
    glm::vec3 blend = glm::smoothstep(glm::vec3(0.0), glm::vec3(1.0), p_fract);

    // Get the 8 corner coordinates of the cube
    glm::vec3 c000 = p_int;
    glm::vec3 c100 = p_int + glm::vec3(1, 0, 0);
    glm::vec3 c010 = p_int + glm::vec3(0, 1, 0);
    glm::vec3 c110 = p_int + glm::vec3(1, 1, 0);
    glm::vec3 c001 = p_int + glm::vec3(0, 0, 1);
    glm::vec3 c101 = p_int + glm::vec3(1, 0, 1);
    glm::vec3 c011 = p_int + glm::vec3(0, 1, 1);
    glm::vec3 c111 = p_int + glm::vec3(1, 1, 1);

    // Sample noise at the 8 corners
    float n000 = Simplex::noise(glm::mod(c000 / period, glm::vec3(1.0)));
    float n100 = Simplex::noise(glm::mod(c100 / period, glm::vec3(1.0)));
    float n010 = Simplex::noise(glm::mod(c010 / period, glm::vec3(1.0)));
    float n110 = Simplex::noise(glm::mod(c110 / period, glm::vec3(1.0)));
    float n001 = Simplex::noise(glm::mod(c001 / period, glm::vec3(1.0)));
    float n101 = Simplex::noise(glm::mod(c101 / period, glm::vec3(1.0)));
    float n011 = Simplex::noise(glm::mod(c011 / period, glm::vec3(1.0)));
    float n111 = Simplex::noise(glm::mod(c111 / period, glm::vec3(1.0)));

    // Trilinear interpolation
    float nx00 = glm::mix(n000, n100, blend.x);
    float nx10 = glm::mix(n010, n110, blend.x);
    float nx01 = glm::mix(n001, n101, blend.x);
    float nx11 = glm::mix(n011, n111, blend.x);

    float nxy0 = glm::mix(nx00, nx10, blend.y);
    float nxy1 = glm::mix(nx01, nx11, blend.y);

    return glm::mix(nxy0, nxy1, blend.z);
}

float fbm(glm::vec3 p, float period, int octaves, float persistence, float lacunarity) {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; i++) {
        total += tileableNoise(p * frequency, period) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }

    return total / maxValue;
}

int main() {
    std::cout << "Generating 3D looping noise texture..." << std::endl;

    for (int z = 0; z < TEXTURE_DEPTH; ++z) {
        std::vector<unsigned char> imageData(TEXTURE_WIDTH * TEXTURE_HEIGHT);
        for (int y = 0; y < TEXTURE_HEIGHT; ++y) {
            for (int x = 0; x < TEXTURE_WIDTH; ++x) {
                glm::vec3 p(
                    (float)x / TEXTURE_WIDTH,
                    (float)y / TEXTURE_HEIGHT,
                    (float)z / TEXTURE_DEPTH
                );

                float noiseVal = fbm(
                    p,
                    1.0f,  // period
                    4,     // octaves
                    0.5f,  // persistence
                    2.0f   // lacunarity
                );

                // Normalize noise from [-1, 1] to [0, 255]
                unsigned char pixelVal = static_cast<unsigned char>((noiseVal * 0.5f + 0.5f) * 255.0f);
                imageData[y * TEXTURE_WIDTH + x] = pixelVal;
            }
        }

        char filepath[100];
        snprintf(filepath, sizeof(filepath), "assets/textures/noise/noise_slice_%03d.png", z);
        stbi_write_png(filepath, TEXTURE_WIDTH, TEXTURE_HEIGHT, 1, imageData.data(), TEXTURE_WIDTH);

        if (z % 10 == 0) {
            std::cout << "Generated slice " << z << "/" << TEXTURE_DEPTH << std::endl;
        }
    }

    std::cout << "Finished generating noise texture." << std::endl;
    return 0;
}
