#pragma once

#include "IWidget.h"
#include "TreeManager.h"

namespace Boidsish {
namespace UI {

class TreeWidget : public IWidget {
public:
    TreeWidget(TreeManager& treeManager);
    ~TreeWidget();

    void Draw() override;

private:
    TreeManager& m_treeManager;
};

} // namespace UI
} // namespace Boidsish
