#include "ui/Widgets.h"

#include <algorithm>

#include "imgui.h"

namespace Boidsish {
	namespace UI {
		void SliderFloatWithActual(const char* label, float* target, float actual, float min, float max) {
			ImGui::SliderFloat(label, target, min, max);

			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			ImVec2      widget_pos = ImGui::GetItemRectMin();
			ImVec2      widget_size = ImGui::GetItemRectSize();

			float fraction = (actual - min) / (max - min);
			fraction = std::clamp(fraction, 0.0f, 1.0f);

			float marker_width = 8.0f;
			float marker_height = 10.0f;
			float marker_y = widget_pos.y + widget_size.y * 0.5f; // Center vertically
			float marker_x = widget_pos.x + (widget_size.x * fraction);

			ImVec2 p1 = ImVec2(marker_x, marker_y + marker_height);
			ImVec2 p2 = ImVec2(marker_x - marker_width, marker_y - marker_height);
			ImVec2 p3 = ImVec2(marker_x + marker_width, marker_y - marker_height);

			draw_list->AddTriangleFilled(p1, p2, p3, IM_COL32(0, 255, 255, 200));
			draw_list->AddTriangle(p1, p2, p3, IM_COL32(255, 255, 255, 200), 1.0f);
		}
	} // namespace UI
} // namespace Boidsish
