#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "graphics.h"
#include "decor_manager.h"
#include "terrain_generator.h"
#include "terrain_render_manager.h"
#include "shader.h"

using namespace Boidsish;

int main() {
    // 1. Initialize Visualizer
    Visualizer visualizer(1280, 720, "Decor LBVH Raytracing");

    // 2. Setup Terrain and Decor
    auto terrain_gen = visualizer.SetTerrainGenerator<TerrainGenerator>();
    auto decor_manager = visualizer.GetDecorManager();
    if (!decor_manager) {
        std::cerr << "Decor manager not found" << std::endl;
        return -1;
    }
    decor_manager->PopulateDefaultDecor();

    // 3. Setup Camera
    Camera cam;
    cam.x = 0.0f;
    cam.y = 50.0f;
    cam.z = 0.0f;
    cam.pitch = -30.0f;
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

    std::cout << "Starting main loop..." << std::endl;

    // Run for a few frames to let things stabilize/initialize
    for (int frame = 0; frame < 10; ++frame) {
        // Update visualizer (this updates terrain and decor internally)
        visualizer.Update();

        glm::mat4 view = visualizer.GetViewMatrix();
        glm::mat4 proj = visualizer.GetProjectionMatrix();
        glm::vec3 cameraPos = visualizer.GetCamera().pos();

        // Clear textures
        float clearColor[4] = {0.1f, 0.1f, 0.1f, 1.0f};
        glClearTexImage(output_tex, 0, GL_RGBA, GL_FLOAT, clearColor);
        float clearDepth = 1e30f;
        glClearTexImage(depth_tex, 0, GL_RED, GL_FLOAT, &clearDepth);

        // Raytrace against each decor type's LBVH
        for (const auto& type : decor_manager->GetDecorTypes()) {
            if (type.lbvh && type.lbvh->GetNumObjects() > 0) {
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

        std::cout << "Frame " << frame << " complete." << std::endl;
    }

    std::cout << "Decor Raytracing Example complete." << std::endl;

    return 0;
}
