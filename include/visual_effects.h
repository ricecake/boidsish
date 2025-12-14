#pragma once

namespace Boidsish {
    enum class VisualEffect {
        RIPPLE,
        COLOR_SHIFT,
        // Add other effects here
    };

    struct VisualEffectsUbo {
        int ripple_enabled;
        int color_shift_enabled;
        // Add other effect states here
    };
}
