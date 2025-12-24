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

        private:
            std::vector<std::shared_ptr<IWidget>> m_widgets;
        };
    }
}
