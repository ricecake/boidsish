#ifndef HELPERS_OCTAHEDRAL_GLSL
#define HELPERS_OCTAHEDRAL_GLSL

/**
 * Wraps octahedral coordinates for the bottom hemisphere.
 */
vec2 octWrap(vec2 v) {
    return (1.0 - abs(v.yx)) * (vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0));
}

/**
 * Encodes a unit vector into a 2D octahedral coordinate [0, 1].
 */
vec2 octEncode(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    n.xy = n.z >= 0.0 ? n.xy : octWrap(n.xy);
    return n.xy * 0.5 + 0.5;
}

/**
 * Decodes a 2D octahedral coordinate [0, 1] into a unit vector.
 */
vec3 octDecode(vec2 f) {
    f = f * 2.0 - 1.0;
    // https://twitter.com/Stubbesaurus/status/406798031268802560
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = clamp(-n.z, 0.0, 1.0);
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize(n);
}

#endif // HELPERS_OCTAHEDRAL_GLSL
