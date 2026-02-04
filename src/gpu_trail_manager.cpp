#include "gpu_trail_manager.h"
#include "shader.h"
#include "logger.h"
#include <algorithm>

namespace Boidsish {

    GpuTrailManager::GpuTrailManager() {}

    GpuTrailManager::~GpuTrailManager() {
        if (control_points_ssbo_) glDeleteBuffers(1, &control_points_ssbo_);
        if (segments_ssbo_) glDeleteBuffers(1, &segments_ssbo_);
        if (trail_info_ssbo_) glDeleteBuffers(1, &trail_info_ssbo_);
        if (proxy_vao_) glDeleteVertexArrays(1, &proxy_vao_);
        if (proxy_vbo_) glDeleteBuffers(1, &proxy_vbo_);
    }

    void GpuTrailManager::Initialize() {
        if (initialized_) return;

        // Initialize SSBOs
        glGenBuffers(1, &control_points_ssbo_);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, control_points_ssbo_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxTrails * kMaxPointsPerTrail * sizeof(GpuTrailControlPoint), nullptr, GL_DYNAMIC_DRAW);

        glGenBuffers(1, &segments_ssbo_);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, segments_ssbo_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxSegments * sizeof(GpuTrailSegment), nullptr, GL_DYNAMIC_DRAW);
        // Clear segments buffer to avoid garbage data
        glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);

        glGenBuffers(1, &trail_info_ssbo_);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, trail_info_ssbo_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxTrails * sizeof(GpuTrailInfo), nullptr, GL_DYNAMIC_DRAW);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        // Shaders
        compute_shader_ = std::make_unique<ComputeShader>("shaders/gpu_trail.comp");
        render_shader_ = std::make_unique<Shader>("shaders/gpu_trail.vert", "shaders/gpu_trail.frag");

        // Proxy geometry (a unit cube/box for each segment)
        float vertices[] = {
            -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
             0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,

            -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
             0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f,

            -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
            -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,

             0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,
             0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,

            -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,
             0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f,

            -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,
             0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f,
        };

        glGenVertexArrays(1, &proxy_vao_);
        glGenBuffers(1, &proxy_vbo_);

        glBindVertexArray(proxy_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, proxy_vbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        initialized_ = true;
        logger::INFO("GpuTrailManager initialized");
    }

    int GpuTrailManager::AddTrail(int max_points) {
        if (trails_.size() >= kMaxTrails) return -1;

        int id = next_trail_id_++;
        TrailData data;
        data.max_count = std::min(max_points, kMaxPointsPerTrail);
        data.points.resize(data.max_count);

        // Assign offsets
        int index = 0;
        // Find first free index
        std::vector<bool> used(kMaxTrails, false);
        for (const auto& [tid, tdata] : trails_) {
            used[tdata.offset / kMaxPointsPerTrail] = true;
        }
        for (int i = 0; i < kMaxTrails; ++i) {
            if (!used[i]) {
                index = i;
                break;
            }
        }

        data.offset = index * kMaxPointsPerTrail;
        data.segments_offset = index * kMaxPointsPerTrail * kInterpolationFactor;

        trails_[id] = data;
        return id;
    }

    void GpuTrailManager::RemoveTrail(int trail_id) {
        trails_.erase(trail_id);
    }

    void GpuTrailManager::AddPoint(int trail_id, glm::vec3 pos, glm::vec3 color, float thickness, float time) {
        auto it = trails_.find(trail_id);
        if (it == trails_.end()) return;

        auto& data = it->second;
        data.points[data.head] = { glm::vec4(pos, thickness), glm::vec4(color, time) };
        data.head = (data.head + 1) % data.max_count;
        if (data.count < data.max_count) data.count++;
        data.dirty = true;
    }

    void GpuTrailManager::Update(float delta_time, float simulation_time) {
        if (!initialized_) return;

        UpdateBuffers();

        // Dispatch compute shader
        compute_shader_->use();
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::GpuTrailControlPoints(), control_points_ssbo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::GpuTrailSegments(), segments_ssbo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::GpuTrailInfo(), trail_info_ssbo_);

        compute_shader_->setInt("interpolationFactor", kInterpolationFactor);

        // Dispatch one work group per trail
        if (!trails_.empty()) {
            glDispatchCompute(static_cast<GLuint>(kMaxTrails), 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        }
    }

    void GpuTrailManager::UpdateBuffers() {
        std::vector<GpuTrailInfo> info_data(kMaxTrails);
        for (int i = 0; i < kMaxTrails; ++i) {
            info_data[i].isActive = 0;
            info_data[i].offset = i * kMaxPointsPerTrail;
            info_data[i].max_count = kMaxPointsPerTrail;
            info_data[i].segments_offset = i * kMaxPointsPerTrail * kInterpolationFactor;
        }

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, control_points_ssbo_);
        for (auto& [id, data] : trails_) {
            if (data.dirty) {
                glBufferSubData(GL_SHADER_STORAGE_BUFFER, data.offset * sizeof(GpuTrailControlPoint), data.max_count * sizeof(GpuTrailControlPoint), data.points.data());
                data.dirty = false;
            }

            int index = data.offset / kMaxPointsPerTrail;
            info_data[index].offset = data.offset;
            info_data[index].count = data.count;
            info_data[index].head = data.head;
            info_data[index].max_count = data.max_count;
            info_data[index].segments_offset = data.segments_offset;
            info_data[index].isActive = 1;
        }

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, trail_info_ssbo_);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, info_data.size() * sizeof(GpuTrailInfo), info_data.data());
    }

    void GpuTrailManager::Render(const glm::mat4& view, const glm::mat4& projection) {
        if (!initialized_ || trails_.empty()) return;

        int max_segment_index = 0;
        for (const auto& [id, data] : trails_) {
            max_segment_index = std::max(max_segment_index, data.segments_offset + data.max_count * kInterpolationFactor);
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        // Use front-face culling to render only the back faces of the proxy boxes.
        // This allows the camera to be inside the box and ensures we have a valid
        // exit point for raymarching (the back face).
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);

        render_shader_->use();
        render_shader_->setMat4("view", view);
        render_shader_->setMat4("projection", projection);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::GpuTrailSegments(), segments_ssbo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::GpuTrailInfo(), trail_info_ssbo_);

        glBindVertexArray(proxy_vao_);
        // Draw up to the last active segment.
        glDrawArraysInstanced(GL_TRIANGLES, 0, 36, max_segment_index);

        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
        glCullFace(GL_BACK); // Restore default
    }

} // namespace Boidsish
