#pragma once

#include "ui/Widget.h"
#include <map>
#include <string>

class PerfCounterWidget : public Widget {
public:
    PerfCounterWidget();
    void Draw() override;
};
