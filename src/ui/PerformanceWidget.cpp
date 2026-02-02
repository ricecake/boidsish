#include "ui/PerformanceWidget.h"
#include "profiler.h"
#include <imgui.h>
#include <numeric>
#include <algorithm>

namespace Boidsish {
    namespace UI {
        PerformanceWidget::PerformanceWidget() {}

        void PerformanceWidget::Draw() {
#ifdef PROFILING_ENABLED
            ImGui::Begin("Performance Profiler");

            auto stats = Profiler::ProfileManager::GetInstance().GetStats();

            if (ImGui::Button("Clear Data")) {
                Profiler::ProfileManager::GetInstance().Clear();
            }

            ImGui::Separator();

            for (auto const& [name, stat] : stats) {
                if (stat.count == 0) continue;

                double avg = stat.total_ms / stat.count;

                ImGui::Text("%s:", name.c_str());
                ImGui::SameLine(200);
                ImGui::Text("Avg: %.3f ms | Max: %.3f ms | Count: %llu", avg, stat.max_ms, (unsigned long long)stat.count);
            }

            ImGui::End();
#endif
        }
    }
}
