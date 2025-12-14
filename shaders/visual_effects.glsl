layout(std140, binding = 1) uniform VisualEffects {
    bool  ripple_enabled;
    float ripple_strength;
    bool  color_shift_enabled;
    float color_shift_strength;
};