#pragma once

#include "IWidget.h"
#include <memory>

namespace Boidsish {
    namespace UI {
        class PerformanceWidget : public IWidget {
        public:
            PerformanceWidget();
            void Draw() override;
        };
    }
}
