#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "graphics.h"
#include "decor_manager.h"
#include "terrain_generator.h"
#include "terrain_render_manager.h"
#include "shader.h"
#include <thread>
#include <chrono>

// Prototype for stb_image_write (it's already in libboidsish.a)
extern "C" int stbi_write_png(char const *filename, int w, int h, int comp, void const *data, int stride_in_bytes);

using namespace Boidsish;

int main() {
    std::cout << "Starting Decor LBVH Raytracing Example..." << std::endl;

    // 1. Initialize Visualizer
    Visualizer visualizer(1280, 720, "Decor LBVH Raytracing");

    // 2. Setup Terrain and Decor
    auto terrain_gen = visualizer.SetTerrainGenerator<TerrainGenerator>();
    auto decor_manager = visualizer.GetDecorManager();
    if (!decor_manager) {
        std::cerr << "Decor manager not found" << std::endl;
        return -1;
    }

    // Increase density for the example and allow all biomes and height ranges
    DecorProperties tree_props = DecorManager::GetDefaultTreeProperties();
    tree_props.min_density = 0.9f;
    tree_props.max_density = 1.0f;
    tree_props.min_height = -500.0f;
    tree_props.max_height = 1000.0f;
    tree_props.biomes = {Biome::LushGrass, Biome::Forest, Biome::AlpineMeadow, Biome::DryGrass, Biome::BrownRock, Biome::GreyRock, Biome::Snow};
    decor_manager->AddDecorType("assets/decor/Tree/tree01.obj", tree_props);

    // 3. Setup Camera - look from above at the center
    Camera cam;
    cam.x = 0.0f;
    cam.y = 100.0f;
    cam.z = 100.0f;
    cam.pitch = -45.0f;
    cam.yaw = 0.0f;
    visualizer.SetCamera(cam);

    // 4. Create Raytracing Shader
    ComputeShader raytrace_shader("shaders/examples/decor_raytrace.comp");
    if (!raytrace_shader.isValid()) {
        std::cerr << "Failed to load raytrace shader" << std::endl;
        return -1;
    }

    // Output textures (color and depth)
    GLuint output_tex;
    glGenTextures(1, &output_tex);
    glBindTexture(GL_TEXTURE_2D, output_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1280, 720, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLuint depth_tex;
    glGenTextures(1, &depth_tex);
    glBindTexture(GL_TEXTURE_2D, depth_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 1280, 720, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    std::cout << "Warming up and generating decor..." << std::endl;

    // Run for enough frames to ensure terrain chunks are loaded and decor is generated
    for (int frame = 0; frame < 60; ++frame) {
        visualizer.Update();
        visualizer.Render();

        if (frame % 20 == 0) {
            int total_objects = 0;
            for (const auto& type : decor_manager->GetDecorTypes()) {
                if (type.lbvh) {
                    total_objects += type.lbvh->GetNumObjects();

                    // Diagnostic: Read back root AABB
                    LBVHNode root;
                    glBindBuffer(GL_SHADER_STORAGE_BUFFER, type.lbvh->GetNodesBuffer());
                    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(LBVHNode), &root);
                    std::cout << "  Type root AABB: (" << root.min_pt_left.x << "," << root.min_pt_left.y << "," << root.min_pt_left.z
                              << ") to (" << root.max_pt_right.x << "," << root.max_pt_right.y << "," << root.max_pt_right.z << ")" << std::endl;
                }
            }
            std::cout << "Frame " << frame << ", total potential objects in LBVHs: " << total_objects << std::endl;
        }
    }

    std::cout << "Starting raytrace..." << std::endl;

    glm::mat4 view = visualizer.GetViewMatrix();
    glm::mat4 proj = visualizer.GetProjectionMatrix();
    glm::vec3 cameraPos = visualizer.GetCamera().pos();

    // Clear textures
    float clearColor[4] = {0.1f, 0.15f, 0.2f, 1.0f}; // Dark blue sky
    glClearTexImage(output_tex, 0, GL_RGBA, GL_FLOAT, clearColor);
    float clearDepth = 1e30f;
    glClearTexImage(depth_tex, 0, GL_RED, GL_FLOAT, &clearDepth);

    // Raytrace against each decor type's LBVH
    int types_traced = 0;
    for (const auto& type : decor_manager->GetDecorTypes()) {
        if (type.lbvh && type.lbvh->GetNumObjects() > 0) {
            types_traced++;
            raytrace_shader.use();
            raytrace_shader.setVec3("u_cameraPos", cameraPos);
            raytrace_shader.setMat4("u_invView", glm::inverse(view));
            raytrace_shader.setMat4("u_invProj", glm::inverse(proj));

            glBindImageTexture(0, output_tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
            glBindImageTexture(1, depth_tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);

            type.lbvh->Bind(20);

            glDispatchCompute(1280/16, 720/16, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        }
    }

    std::cout << "Raytraced against " << types_traced << " decor types." << std::endl;

    // Save output texture to PNG
    std::vector<float> pixels(1280 * 720 * 4);
    glBindTexture(GL_TEXTURE_2D, output_tex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());

    std::vector<unsigned char> png_data(1280 * 720 * 3);
    for (int y = 0; y < 720; ++y) {
        for (int x = 0; x < 1280; ++x) {
            int idx = (y * 1280 + x);
            // Flip Y for STB
            int target_idx = ((719 - y) * 1280 + x);
            png_data[target_idx * 3 + 0] = static_cast<unsigned char>(glm::clamp(pixels[idx * 4 + 0], 0.0f, 1.0f) * 255.0f);
            png_data[target_idx * 3 + 1] = static_cast<unsigned char>(glm::clamp(pixels[idx * 4 + 1], 0.0f, 1.0f) * 255.0f);
            png_data[target_idx * 3 + 2] = static_cast<unsigned char>(glm::clamp(pixels[idx * 4 + 2], 0.0f, 1.0f) * 255.0f);
        }
    }

    stbi_write_png("raytrace_result.png", 1280, 720, 3, png_data.data(), 1280 * 3);

    std::cout << "Decor Raytracing Example complete. Saved result to raytrace_result.png" << std::endl;

    return 0;
}
