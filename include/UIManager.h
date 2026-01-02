#pragma once

#include <functional>
#include <memory>
#include <vector>

struct GLFWwindow;

namespace Boidsish {
	namespace UI {
		class IWidget;

		class UIManager {
		public:
			UIManager(GLFWwindow* window);
			~UIManager();

			void AddWidget(std::shared_ptr<IWidget> widget);
			void Render();
			void ToggleMenus() { m_show_menus = !m_show_menus; }

		private:
			std::vector<std::shared_ptr<IWidget>> m_widgets;
			bool m_show_menus = false;
		};
	} // namespace UI
} // namespace Boidsish
