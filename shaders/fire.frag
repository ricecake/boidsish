#version 430 core

in float v_lifetime;
out vec4 FragColor;

void main() {
    // Shape the point into a circle and discard fragments outside the circle
    vec2 circ = gl_PointCoord - vec2(0.5);
    float dist = dot(circ, circ);
    if (dist > 0.25) {
        discard;
    }

    // Color gradient for fire
    vec3 hot_color = vec3(1.0, 1.0, 0.6);   // Bright yellow-white
    vec3 mid_color = vec3(1.0, 0.5, 0.0);   // Orange
    vec3 cool_color = vec3(0.4, 0.1, 0.0);    // Dark red/smokey

    // Interpolate color based on lifetime
    vec3 color = mix(mid_color, hot_color, v_lifetime * v_lifetime); // Hotter when new
    color = mix(cool_color, color, v_lifetime); // Cooler as it dies

    // Fade out alpha based on lifetime and distance from center
    float alpha = (1.0 - (dist / 0.25)) * v_lifetime;

    FragColor = vec4(color, alpha);
}
