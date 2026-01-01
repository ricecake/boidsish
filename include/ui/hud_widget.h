#pragma once

#include "IWidget.h"
#include <memory>

namespace Boidsish {

class HudManager;

namespace UI {

class HudWidget : public IWidget {
public:
    HudWidget(HudManager& hudManager);
    void Draw() override;

private:
    HudManager& m_hudManager;
};

} // namespace UI
} // namespace Boidsish
