// D2Q9 lattice definitions
const vec2 e[9] = vec2[](
    vec2(0,0), vec2(1,0), vec2(0,1), vec2(-1,0), vec2(0,-1),
    vec2(1,1), vec2(-1,1), vec2(-1,-1), vec2(1,-1)
);

const float w[9] = float[](
    4.0/9.0, 1.0/9.0, 1.0/9.0, 1.0/9.0, 1.0/9.0,
    1.0/36.0, 1.0/36.0, 1.0/36.0, 1.0/36.0
);

const int opp[9] = int[](0, 3, 4, 1, 2, 7, 8, 5, 6);

struct LbmCell {
    float f[9];
    float temperature;
    float aerosol;
    float vVel;
};

struct AerosolSource {
    vec2  position;
    float strength;
    float radius;
};

layout(std430, binding = [[WEATHER_GRID_A_BINDING]]) buffer GridA {
    LbmCell cellsA[];
};

layout(std430, binding = [[WEATHER_GRID_B_BINDING]]) buffer GridB {
    LbmCell cellsB[];
};

layout(std140, binding = 32) uniform WeatherUniforms {
    vec4 u_weatherOriginAndSize; // xy = origin, zw = size
    vec4 u_weatherParams;        // x = cellSize, y = globalTemp, zw = unused
};

#define u_width  uint(u_weatherOriginAndSize.z)
#define u_height uint(u_weatherOriginAndSize.w)
#define u_cellSize u_weatherParams.x
#define u_globalTemperature u_weatherParams.y

uniform float u_deltaTime;

uint getIdx(uint x, uint y) {
    return y * u_width + x;
}

float getEquilibrium(int i, float rho, vec2 u) {
    float eu = dot(e[i], u);
    float u2 = dot(u, u);
    return w[i] * rho * (1.0 + 3.0 * eu + 4.5 * eu * eu - 1.5 * u2);
}
