#pragma once

#include "shape.h"

enum class PlatonicSolidType {
    TETRAHEDRON,
    CUBE,
    OCTAHEDRON,
    DODECAHEDRON,
    ICOSAHEDRON
};

class PlatonicSolid : public Boidsish::Shape {
public:
    PlatonicSolid(PlatonicSolidType type, int id = 0, float x = 0.0f,
                  float y = 0.0f, float z = 0.0f, float r = 1.0f,
                  float g = 1.0f, float b = 1.0f, float a = 1.0f,
                  int trail_length = 0);
    ~PlatonicSolid() override = default;

    void render() const override;

    static void Init();
    static void Cleanup();

private:
    PlatonicSolidType type_;
};
