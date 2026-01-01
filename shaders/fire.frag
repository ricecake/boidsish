#version 430 core

in float v_lifetime;
out vec4 FragColor;

uniform int u_style;

void main() {
    // Shape the point into a circle and discard fragments outside the circle
    vec2 circ = gl_PointCoord - vec2(0.5);
    float dist = dot(circ, circ);
    if (dist > 0.25) {
        discard;
    }

    vec3 color;
    if (u_style == 0) { // Default Fire
        vec3 hot_color = vec3(1.0, 1.0, 0.6);   // Bright yellow-white
        vec3 mid_color = vec3(1.0, 0.5, 0.0);   // Orange
        vec3 cool_color = vec3(0.4, 0.1, 0.0);    // Dark red/smokey
        vec3 smoke_color = vec3(0.2, 0.2, 0.2);    // Dark red/smokey
        color = mix(mix(smoke_color, cool_color, v_lifetime), mix(mid_color, hot_color, v_lifetime*v_lifetime), v_lifetime);
    } else if (u_style == 1) { // Rocket Trail
        vec3 hot_color = vec3(0.8, 0.8, 1.0);   // Bright blue-white
        vec3 mid_color = vec3(0.5, 0.5, 1.0);   // Blue
        vec3 cool_color = vec3(0.1, 0.1, 0.3);    // Dark blue
        vec3 smoke_color = vec3(0.3, 0.3, 0.3);
        color = mix(mix(smoke_color, cool_color, v_lifetime), mix(mid_color, hot_color, v_lifetime * v_lifetime), v_lifetime);
    } else if (u_style == 2) { // Explosion
        vec3 hot_color = vec3(1.0, 1.0, 0.8);
        vec3 mid_color = vec3(1.0, 0.8, 0.2);
        vec3 cool_color = vec3(0.8, 0.2, 0.0);
        vec3 smoke_color = vec3(0.4, 0.4, 0.4);
        color = mix(mix(smoke_color, cool_color, v_lifetime), mix(mid_color, hot_color, v_lifetime * v_lifetime), v_lifetime);
    }

    float alpha = smoothstep((1.0 - v_lifetime), (v_lifetime), v_lifetime / 2.5);

    FragColor = vec4(color, alpha);
}
