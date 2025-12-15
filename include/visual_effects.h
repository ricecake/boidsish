#pragma once

#include <map>

namespace Boidsish {

// Enum representing all possible visual effects
enum class VisualEffect {
    RIPPLE,
    COLOR_SHIFT,
    // Add other effects here
};

// Parameters for each visual effect
struct EffectParameters {
    float strength = 1.0f;
    // Add other parameters as needed, e.g., color, speed
};

// State of an effect in the hierarchy
enum class EffectState {
    DEFAULT,  // Inherit from the parent
    ENABLED,  // Force enable
    DISABLED, // Force disable
};

// Settings for a single visual effect
struct EffectSettings {
    EffectState      state = EffectState::DEFAULT;
    EffectParameters params;
};

// A collection of effect settings for an object or handler
class EffectSet {
public:
    // Set the state of a specific effect
    void SetEffectState(VisualEffect effect, EffectState state) {
        settings_[effect].state = state;
    }

    // Set the parameters for a specific effect
    void SetEffectParameters(VisualEffect effect, const EffectParameters& params) {
        settings_[effect].params = params;
    }

    // Get the settings for a specific effect
    const EffectSettings& GetEffectSettings(VisualEffect effect) const {
        auto it = settings_.find(effect);
        if (it != settings_.end()) {
            return it->second;
        }
        return default_settings_;
    }

    // Resolve the final effect settings by merging multiple sets
    static EffectSet Resolve(const EffectSet& global, const EffectSet& handler, const EffectSet& local) {
        EffectSet resolved;
        for (int i = 0; i < static_cast<int>(VisualEffect::COLOR_SHIFT) + 1; ++i) {
            VisualEffect effect = static_cast<VisualEffect>(i);

            // Determine the final state based on hierarchy: local -> handler -> global
            EffectState final_state = local.GetEffectSettings(effect).state;
            if (final_state == EffectState::DEFAULT) {
                final_state = handler.GetEffectSettings(effect).state;
            }
            if (final_state == EffectState::DEFAULT) {
                final_state = global.GetEffectSettings(effect).state;
            }

            // Determine the final parameters (local overrides handler, which overrides global)
            EffectParameters final_params = global.GetEffectSettings(effect).params;
            final_params = handler.GetEffectSettings(effect).params; // Simplistic override, could be blended
            final_params = local.GetEffectSettings(effect).params;

            resolved.SetEffectState(effect, final_state);
            resolved.SetEffectParameters(effect, final_params);
        }
        return resolved;
    }

private:
    std::map<VisualEffect, EffectSettings> settings_;
    static const EffectSettings            default_settings_;
};

// UBO structure for visual effects, sent to the GPU
struct VisualEffectsUbo {
    int   ripple_enabled;
    float ripple_strength;
    int   color_shift_enabled;
    float color_shift_strength;
    // Add other effect states here (ensure std140 layout compatibility)
};

} // namespace Boidsish