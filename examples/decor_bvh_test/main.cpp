#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "graphics.h"
#include "decor_manager.h"
#include "terrain_generator.h"
#include "terrain_render_manager.h"
#include "asset_manager.h"
#include "logger.h"

using namespace Boidsish;

struct HitResult {
    glm::vec4 hit_pos; // xyz = position, w = distance
    int instance_idx;
    int triangle_idx;
};

int main() {
    // 1. Initialize Visualizer
    Visualizer visualizer(1280, 720, "Decor BVH Test");

    // Set up a basic camera
    Camera camera;
    camera.x = 0; camera.y = 50; camera.z = 100;
    camera.pitch = -0.5f; camera.yaw = 0.0f;

    // 2. Setup Terrain and Decor
    auto terrain_gen = std::make_shared<TerrainGenerator>();
    auto terrain_render = std::make_shared<TerrainRenderManager>();

    DecorManager decor_manager;
    // Add a single decor type (Apple tree is a good complex model)
    decor_manager.AddDecorType("assets/decor/Apple tree/AppleTree.obj", 0.1f);

    // Prepare resources
    // DecorManager::PrepareResources(nullptr) will trigger default GPU upload if no megabuffer is provided
    decor_manager.PrepareResources(nullptr);

    // Force a single update to place some decor
    Frustum frustum = Frustum::FromViewProjection(glm::mat4(1.0f), glm::mat4(1.0f));
    decor_manager.Update(0.16f, camera, frustum, *terrain_gen, terrain_render);

    // 3. Setup Raycast Compute Shader
    ComputeShader raycast_shader("shaders/lbvh/lbvh_decor_test.comp");
    if (!raycast_shader.isValid()) {
        logger::ERROR("Failed to compile raycast shader");
        return -1;
    }

    GLuint hit_ssbo;
    glGenBuffers(1, &hit_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, hit_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(HitResult), nullptr, GL_DYNAMIC_DRAW);

    std::cout << "Starting raycast validation..." << std::endl;

    // 4. Perform Raycasts
    // We'll shoot a grid of rays and see if we hit anything
    int hits = 0;
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 10; ++x) {
            float u = (float)x / 10.0f - 0.5f;
            float v = (float)y / 10.0f - 0.5f;

            glm::vec3 ray_origin(0, 100, 0);
            glm::vec3 ray_dir = glm::normalize(glm::vec3(u, -1.0f, v));

            raycast_shader.use();
            raycast_shader.setVec3("u_rayOrigin", ray_origin);
            raycast_shader.setVec3("u_rayDir", ray_dir);

            // Bind TLAS for the first decor type
            if (decor_manager.GetDecorTypes().empty()) break;
            auto& type = decor_manager.GetDecorTypes()[0];
            if (type.lbvh) type.lbvh->Bind(20);

            // Bind BLAS for the model
            auto model_data = type.model->GetData();
            if (model_data->lbvh) model_data->lbvh->Bind(21);

            // Bind other buffers
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, type.ssbo);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 22, model_data->triangle_indices_ssbo);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 23, model_data->triangle_vertices_ssbo);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 30, hit_ssbo);

            raycast_shader.setInt("u_tlasRoot", type.lbvh ? type.lbvh->GetRootIndex() : -1);
            raycast_shader.setInt("u_blasRoot", model_data->lbvh ? model_data->lbvh->GetRootIndex() : -1);

            glDispatchCompute(1, 1, 1);
            glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

            HitResult result;
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, hit_ssbo);
            glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(HitResult), &result);

            if (result.instance_idx != -1) {
                hits++;
                std::cout << "Hit instance " << result.instance_idx << " triangle " << result.triangle_idx
                          << " at distance " << result.hit_pos.w << std::endl;
            }
        }
    }

    std::cout << "Total hits: " << hits << std::endl;
    if (hits > 0) {
        std::cout << "Decor BVH Test PASSED." << std::endl;
    } else {
        std::cout << "Decor BVH Test FAILED (no hits recorded)." << std::endl;
    }

    return 0;
}
