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

            bool IsVisible() const override { return m_show; }
            void SetVisible(bool visible) override { m_show = visible; }

        private:
            Visualizer& m_visualizer;
            bool m_show = true;
        };
    }
}
