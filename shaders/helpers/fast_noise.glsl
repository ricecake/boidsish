// Helper functions for fast texture-based noise lookups
// Requires a 3D texture sampler named 'u_noiseTexture' bound to some unit.

uniform sampler3D u_noiseTexture;

// R: Simplex 3D
float fastSimplex3d(vec3 p) {
    return texture(u_noiseTexture, p).r * 2.0 - 1.0;
}

// G: Worley 3D
float fastWorley3d(vec3 p) {
    return texture(u_noiseTexture, p).g;
}

// B: FBM 3D
float fastFbm3d(vec3 p) {
    return texture(u_noiseTexture, p).b * 2.0 - 1.0;
}

// A: Warped FBM 3D
float fastWarpedFbm3d(vec3 p) {
    return texture(u_noiseTexture, p).a * 2.0 - 1.0;
}

// Multi-octave texture FBM
float fastTextureFbm(vec3 p, int octaves) {
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < octaves; i++) {
        value += amplitude * (texture(u_noiseTexture, p).r * 2.0 - 1.0);
        p *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}
