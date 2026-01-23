#ifndef LIGHTS_WIDGET_H
#define LIGHTS_WIDGET_H

#include "IWidget.h"
#include "light_manager.h"

namespace Boidsish {
namespace UI {

class LightsWidget : public IWidget {
public:
    LightsWidget(LightManager& lightManager);
    void Draw() override;

private:
    LightManager& _lightManager;
};

} // namespace UI
} // namespace Boidsish

#endif // LIGHTS_WIDGET_H
