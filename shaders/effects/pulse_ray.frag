#version 460 core

in float vIntensity;
out float FragColor;

void main() {
    // Round point to circle
    vec2 circCoord = 2.0 * gl_PointCoord - 1.0;
    if (dot(circCoord, circCoord) > 1.0) discard;

    FragColor = vIntensity;
}
