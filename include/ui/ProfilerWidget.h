#pragma once

#include "IWidget.h"
#include <string>

namespace Boidsish {
    namespace UI {
        class ProfilerWidget : public IWidget {
        public:
            ProfilerWidget();
            void Draw() override;
            bool IsHud() const override { return false; }

            void SetVisible(bool visible);
            bool IsVisible() const;

        private:
            bool m_show = false;
        };
    }
}
