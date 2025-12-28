#pragma once

#include "IWidget.h"
#include "ReactionDiffusionManager.h"

namespace Boidsish {
namespace UI {

    class ReactionDiffusionWidget : public IWidget {
    public:
        ReactionDiffusionWidget(ReactionDiffusionManager& manager);
        void Draw() override;

    private:
        ReactionDiffusionManager& m_manager;
        std::string m_name;
    };

}
}
