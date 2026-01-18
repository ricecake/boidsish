#include "ui/PerfCounterWidget.h"
#include "perf_counter.h"
#include "imgui.h"

PerfCounterWidget::PerfCounterWidget() {}

void PerfCounterWidget::Draw() {
    ImGui::Begin("Performance");
    ImGui::Text("Scope Times (ms):");
    for (auto const& [name, time] : PerfCounter::GetScopeTimes()) {
        ImGui::Text("%s: %.3f", name.c_str(), time);
    }
    ImGui::Separator();
    ImGui::Text("Counts:");
    for (auto const& [name, count] : PerfCounter::GetCounts()) {
        ImGui::Text("%s: %d", name.c_str(), count);
    }
    ImGui::End();
}
