#include "graphics.h"
#include "debug_laser.h"
#include "line.h"
#include "dot.h"
#include "shader.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


namespace Boidsish {

DebugLaser::DebugLaser(Visualizer& visualizer) : visualizer_(visualizer) {
    initial_ray_ = std::make_shared<Line>(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f)); // Red
    reflected_ray_ = std::make_shared<Line>(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // Green
    intersection_dot_ = std::make_shared<Dot>(0, 0, 0, 0, 0.0f, 0.0f, 1.0f); // Blue
    intersection_dot_->SetScale(glm::vec3(1.5f));

    initial_ray_->SetVisible(false);
    reflected_ray_->SetVisible(false);
    intersection_dot_->SetVisible(false);
}

DebugLaser::~DebugLaser() = default;

void DebugLaser::Update(const InputState& input_state, const Camera& camera) {
    if (!is_enabled_) {
        initial_ray_->SetVisible(false);
        reflected_ray_->SetVisible(false);
        intersection_dot_->SetVisible(false);
        return;
    }

    int width = visualizer_.GetWidth();
    int height = visualizer_.GetHeight();

    glm::mat4 projection = glm::perspective(glm::radians(camera.fov), (float)width / (float)height, 0.1f, 1000.0f);
    glm::mat4 view = glm::lookAt(camera.pos(), camera.pos() + camera.front(), camera.up());

    glm::vec3 world_pos_near = glm::unProject(glm::vec3(input_state.mouse_x, height - input_state.mouse_y, 0.0f), view, projection, glm::vec4(0, 0, width, height));
    glm::vec3 world_pos_far = glm::unProject(glm::vec3(input_state.mouse_x, height - input_state.mouse_y, 1.0f), view, projection, glm::vec4(0, 0, width, height));

    glm::vec3 ray_dir = glm::normalize(world_pos_far - world_pos_near);
    glm::vec3 ray_origin = camera.pos();

    glm::vec3 intersection_point, intersection_normal;
    intersection_found_ = RayTerrainIntersection(ray_origin, ray_dir, intersection_point, intersection_normal);

    if (intersection_found_) {
        initial_ray_->SetPoints(ray_origin, intersection_point);

        glm::vec3 reflected_dir = glm::reflect(ray_dir, glm::normalize(intersection_normal));
        reflected_ray_->SetPoints(intersection_point, intersection_point + reflected_dir * 20.0f);

        intersection_dot_->SetPosition(intersection_point.x, intersection_point.y, intersection_point.z);

        initial_ray_->SetVisible(true);
        reflected_ray_->SetVisible(true);
        intersection_dot_->SetVisible(true);
    } else {
        initial_ray_->SetPoints(ray_origin, ray_origin + ray_dir * 1000.0f);

        initial_ray_->SetVisible(true);
        reflected_ray_->SetVisible(false);
        intersection_dot_->SetVisible(false);
    }
}

void DebugLaser::Render(Shader& shader) {
    if (!is_enabled_) return;

    // The line and dot shapes use the global Shape::shader, so we don't need to pass one
    // But we will disable depth testing to make sure the lines are always visible.
    glDisable(GL_DEPTH_TEST);
    glLineWidth(3.0f);
    if (initial_ray_ && initial_ray_->IsVisible()) {
        initial_ray_->render();
    }
    if (reflected_ray_ && reflected_ray_->IsVisible()) {
        reflected_ray_->render();
    }
    if (intersection_dot_ && intersection_dot_->IsVisible()) {
        intersection_dot_->render();
    }
    glLineWidth(1.0f);
    glEnable(GL_DEPTH_TEST);
}


bool DebugLaser::RayTerrainIntersection(const glm::vec3& ray_origin, const glm::vec3& ray_dir, glm::vec3& out_intersection, glm::vec3& out_normal) {
    const float step_size = 0.5f;
    const int max_steps = 1000;
    const float start_offset = 0.1f; // Start search slightly in front of the camera

    float current_dist = start_offset;
    for (int i = 0; i < max_steps; ++i) {
        glm::vec3 current_pos = ray_origin + ray_dir * current_dist;

        if (current_pos.y < -100) return false; // Optimization: stop if we go way below any possible terrain

        auto [terrain_height, terrain_normal] = visualizer_.GetTerrainPointProperties(current_pos.x, current_pos.z);

        if (current_pos.y < terrain_height) {
            // We have an intersection. Now refine it with a few steps of binary search.
            float high = current_dist;
            float low = current_dist - step_size;

            for(int j = 0; j < 5; ++j) { // 5 iterations for refinement
                float mid = (low + high) / 2.0f;
                glm::vec3 mid_pos = ray_origin + ray_dir * mid;
                auto [mid_terrain_height, _] = visualizer_.GetTerrainPointProperties(mid_pos.x, mid_pos.z);
                if (mid_pos.y < mid_terrain_height) {
                    high = mid;
                } else {
                    low = mid;
                }
            }

            out_intersection = ray_origin + ray_dir * high;
            auto [final_height, final_normal] = visualizer_.GetTerrainPointProperties(out_intersection.x, out_intersection.z);
            out_intersection.y = final_height;
            out_normal = final_normal;

            return true;
        }

        current_dist += step_size;
    }

    return false;
}

} // namespace Boidsish
