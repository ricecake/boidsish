#version 460 core

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 Velocity;
layout(location = 2) out vec4 NormalOut;
layout(location = 3) out vec4 AlbedoOut;

in vec3 fColor;
in float fIntensity;
in vec2 fTexCoord;

void main() {
    float d = abs(fTexCoord.x);

    // Core of the lightning is extremely bright and white
    float core = exp(-d * d * 30.0);

    // Envelope/glow of the lightning is wider and colored
    float glow = exp(-d * d * 3.0);

    // Combine core and envelope glow. Core goes to white, glow has the lightning color.
    // We want the center to be very bright (almost white), so we add white core to the colored glow.
    vec3 color = (fColor * glow * 15.0 + vec3(1.0) * core * 20.0) * fIntensity;

    // Fade out towards the edges of the stripe to blend nicely
    float alpha = clamp(glow * 2.0, 0.0, 1.0);

    FragColor = vec4(color, alpha);

    // Non-surface velocity and properties
    Velocity = vec4(0.0, 0.0, 0.0, 0.0);
    NormalOut = vec4(0.0, 0.0, 0.0, 1.0);
    AlbedoOut = vec4(fColor, alpha);
}
