#pragma once

#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "constants.h"

class Shader;
class ComputeShader;

namespace Boidsish {

    struct GpuTrailControlPoint {
        glm::vec4 pos_thickness; // xyz: pos, w: thickness
        glm::vec4 color_time;    // rgb: color, w: time/progress
    };

    struct GpuTrailSegment {
        glm::vec4 p1_thickness1; // xyz: p1, w: thickness1
        glm::vec4 p2_thickness2; // xyz: p2, w: thickness2
        glm::vec4 color;         // rgb: color, w: padding
    };

    struct GpuTrailInfo {
        int offset;     // Offset in ControlPoints SSBO
        int count;      // Number of points
        int head;       // Ring buffer head
        int max_count;  // Max points for this trail
        int segments_offset; // Offset in Segments SSBO
        int isActive;   // 1 if active, 0 otherwise
        int padding[2];
    };

    class GpuTrailManager {
    public:
        GpuTrailManager();
        ~GpuTrailManager();

        void Initialize();

        int  AddTrail(int max_points = Constants::Class::GpuTrails::MaxPointsPerTrail());
        void RemoveTrail(int trail_id);

        void AddPoint(int trail_id, glm::vec3 pos, glm::vec3 color, float thickness, float time);

        void Update(float delta_time, float simulation_time);
        void Render(const glm::mat4& view, const glm::mat4& projection);

        Shader* GetRenderShader() const { return render_shader_.get(); }

    private:
        struct TrailData {
            std::vector<GpuTrailControlPoint> points;
            int head = 0;
            int count = 0;
            int max_count = 0;
            int offset = 0;
            int segments_offset = 0;
            bool dirty = false;
        };

        GLuint control_points_ssbo_ = 0;
        GLuint segments_ssbo_ = 0;
        GLuint trail_info_ssbo_ = 0;

        std::map<int, TrailData> trails_;
        int next_trail_id_ = 0;
        bool initialized_ = false;

        std::unique_ptr<ComputeShader> compute_shader_;
        std::unique_ptr<Shader> render_shader_;

        GLuint proxy_vao_ = 0;
        GLuint proxy_vbo_ = 0;

        void UpdateBuffers();

        static constexpr int kMaxTrails = Boidsish::Constants::Class::GpuTrails::MaxTrails();
        static constexpr int kMaxPointsPerTrail = Boidsish::Constants::Class::GpuTrails::MaxPointsPerTrail();
        static constexpr int kInterpolationFactor = Boidsish::Constants::Class::GpuTrails::InterpolationFactor();
        static constexpr int kMaxSegments = Boidsish::Constants::Class::GpuTrails::MaxSegments();
    };

} // namespace Boidsish
