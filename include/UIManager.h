#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "imgui.h"

struct GLFWwindow;

namespace Boidsish {
	namespace UI {
		class IWidget;

		struct WindowState {
			ImVec2 last_expanded_pos;
			bool   was_collapsed = false;
		};

		class UIManager {
		public:
			UIManager(GLFWwindow* window);
			~UIManager();

			void AddWidget(std::shared_ptr<IWidget> widget);
			void Render();

			template <typename T>
			std::shared_ptr<T> GetWidget() {
				for (auto& widget : m_widgets) {
					if (auto casted = std::dynamic_pointer_cast<T>(widget)) {
						return casted;
					}
				}
				return nullptr;
			}

			void ToggleMenus() { m_show_menus = !m_show_menus; }

		private:
			void PositionMinimizedWindows();

			std::vector<std::shared_ptr<IWidget>>         m_widgets;
			bool                                          m_show_menus = false;
			std::unordered_map<unsigned int, WindowState> m_window_states;
		};
	} // namespace UI
} // namespace Boidsish
