#include "ui/Widgets.h"

#include <algorithm>
#include <cmath>

#include "imgui.h"
#include "imgui_internal.h"

namespace Boidsish {
	namespace UI {

		bool SliderFloatWithActual(const char* label, float* target, float actual, float min, float max) {
			bool changed = ImGui::SliderFloat(label, target, min, max);

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

			return changed;
		}

		bool SliderFloatConstrainedWithActual(const char* label,
		                                      float*      target,
		                                      float       actual,
		                                      float*      constraint_min,
		                                      float*      constraint_max,
		                                      float       min,
		                                      float       max) {
			ImGuiStyle& style = ImGui::GetStyle();
			ImVec2      pos = ImGui::GetCursorScreenPos();
			float       width = ImGui::CalcItemWidth();
			float       height = ImGui::GetFrameHeight();
			ImVec2      size(width, height);

			// Invisible button maps the bounding box and catches mouse inputs
			ImGui::InvisibleButton(label, size);
			bool active = ImGui::IsItemActive();
			bool clicked = ImGui::IsItemClicked();
			bool value_changed = false;

			ImGuiID       id = ImGui::GetItemID();
			ImGuiStorage* storage = ImGui::GetStateStorage();

			auto val_to_x = [&](float v) {
				float fraction = std::clamp((v - min) / (max - min), 0.0f, 1.0f);
				return pos.x + fraction * width;
			};

			// Hit testing: determine which handle was grabbed
			if (clicked) {
				float mouse_x = ImGui::GetIO().MousePos.x;
				float dist_target = std::abs(mouse_x - val_to_x(*target));
				float dist_cmin = std::abs(mouse_x - val_to_x(*constraint_min));
				float dist_cmax = std::abs(mouse_x - val_to_x(*constraint_max));

				int grab_state = 1; // 1: target, 2: min constraint, 3: max constraint

				// 10px grab tolerance for constraint lines, prioritized over target grab
				if (dist_cmin < 10.0f && dist_cmin <= dist_cmax && dist_cmin <= dist_target) {
					grab_state = 2;
				} else if (dist_cmax < 10.0f && dist_cmax <= dist_target) {
					grab_state = 3;
				}

				storage->SetInt(id, grab_state);
			}

			int grab_state = storage->GetInt(id, 0);

			// Handle dragging
			if (active && grab_state != 0) {
				float mouse_x = ImGui::GetIO().MousePos.x - pos.x;
				float fraction = std::clamp(mouse_x / width, 0.0f, 1.0f);
				float new_val = min + fraction * (max - min);

				if (grab_state == 2 && new_val != *constraint_min) {
					*constraint_min = std::clamp(new_val, min, *constraint_max);
					*target = std::max(*target, *constraint_min); // Push target if constraints cross it
					value_changed = true;
				} else if (grab_state == 3 && new_val != *constraint_max) {
					*constraint_max = std::clamp(new_val, *constraint_min, max);
					*target = std::min(*target, *constraint_max); // Push target if constraints cross it
					value_changed = true;
				} else if (grab_state == 1 && new_val != *target) {
					*target = std::clamp(new_val, *constraint_min, *constraint_max); // Lock target inside constraints
					value_changed = true;
				}
			}

			// Rendering
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			ImVec2      p_min = pos;
			ImVec2      p_max = ImVec2(pos.x + width, pos.y + height);

			// 1. Background Track
			draw_list->AddRectFilled(p_min, p_max, ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);

			// 2. Constraint Range Highlight
			float x_cmin = val_to_x(*constraint_min);
			float x_cmax = val_to_x(*constraint_max);
			draw_list->AddRectFilled(ImVec2(x_cmin, p_min.y),
			                         ImVec2(x_cmax, p_max.y),
			                         IM_COL32(100, 150, 250, 60),
			                         style.FrameRounding);

			// 3. Target Handle
			float  x_target = val_to_x(*target);
			ImRect grab_bb(ImVec2(x_target - 4.0f, p_min.y + 2.0f), ImVec2(x_target + 4.0f, p_max.y - 2.0f));
			draw_list->AddRectFilled(
				grab_bb.Min,
				grab_bb.Max,
				ImGui::GetColorU32((active && grab_state == 1) ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab),
				style.GrabRounding);

			// 4. Constraint Handles
			ImU32 constraint_col = IM_COL32(200, 200, 200, 255);
			ImU32 constraint_active_col = IM_COL32(255, 250, 100, 255);
			draw_list->AddLine(ImVec2(x_cmin, p_min.y),
			                   ImVec2(x_cmin, p_max.y),
			                   (active && grab_state == 2) ? constraint_active_col : constraint_col,
			                   2.0f);
			draw_list->AddLine(ImVec2(x_cmax, p_min.y),
			                   ImVec2(x_cmax, p_max.y),
			                   (active && grab_state == 3) ? constraint_active_col : constraint_col,
			                   2.0f);

			// 5. Actual Indicator
			float x_actual = val_to_x(actual);
			float marker_width = 8.0f;
			float marker_height = 10.0f;
			float marker_y = p_min.y + height * 0.5f;

			ImVec2 p1 = ImVec2(x_actual, marker_y + marker_height);
			ImVec2 p2 = ImVec2(x_actual - marker_width, marker_y - marker_height);
			ImVec2 p3 = ImVec2(x_actual + marker_width, marker_y - marker_height);

			draw_list->AddTriangleFilled(p1, p2, p3, IM_COL32(0, 255, 255, 200));
			draw_list->AddTriangle(p1, p2, p3, IM_COL32(255, 255, 255, 200), 1.0f);

			// 6. Label Layout
			ImGui::SameLine(0, style.ItemInnerSpacing.x);
			ImGui::TextUnformatted(label);

			return value_changed;
		}
	} // namespace UI
} // namespace Boidsish