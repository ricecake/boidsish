#version 460 core

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 Velocity;
layout(location = 2) out vec4 NormalOut;
layout(location = 3) out vec4 AlbedoOut;

in vec3 vColor;
in float vIntensity;

void main() {
    // High intensity for bloom
    vec3 color = vColor * vIntensity * 15.0;
    FragColor = vec4(color, 1.0);

    // Non-surface velocity and properties
    Velocity = vec4(0.0, 0.0, 0.0, 0.0);
    NormalOut = vec4(0.0, 0.0, 0.0, 1.0);
    AlbedoOut = vec4(vColor, 1.0);
}
