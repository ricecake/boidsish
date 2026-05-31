#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include "hud_manager.h"
#include "ui/EnvironmentWidget.h"
#include "ui/MoodWidget.h"
#include "ui/LightningWidget.h"
#include "ui/EffectWidget.h"
#include "ui/RenderWidget.h"
#include "ui/AudioWidget.h"
#include "ui/SystemWidget.h"
#include "ui/ProfilerWidget.h"
#include "ui/hud_widget.h"

#include "imgui.h"

struct GLFWwindow;

namespace Boidsish {
	class ServiceLocator;

	namespace UI {
		class IWidget;

		struct WindowState {
			ImVec2 last_expanded_pos;
			ImVec2 last_expanded_size;
			bool   was_collapsed = false;
		};

		class UIConfigManager {
		public:
			UIConfigManager(ServiceLocator& loc, GLFWwindow* window);
			~UIConfigManager();

			void AddWidget(std::shared_ptr<IWidget> widget);
			void SetupDefaultWidgets(class Visualizer& visualizer, class SceneManager& scene_manager, class HudManager& hud_manager);
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
