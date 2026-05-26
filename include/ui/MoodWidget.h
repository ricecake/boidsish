#pragma once

#include "IWidget.h"
#include <string>

namespace Boidsish {
    class Visualizer;
    class MoodManager;

    namespace UI {
        class MoodWidget : public IWidget {
        public:
            MoodWidget(Visualizer& visualizer);
            void Draw() override;

        private:
            Visualizer& m_visualizer;
            bool m_show = true;
        };
    }
}
