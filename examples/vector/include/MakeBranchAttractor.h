#pragma once

#include "vector.h"
#include <random>

namespace Boidsish {

class MakeBranchAttractor {
private:
    std::random_device                    rd;
    std::mt19937                          eng;
    std::uniform_real_distribution<float> x;
    std::uniform_real_distribution<float> y;
    std::uniform_real_distribution<float> z;

public:
    MakeBranchAttractor();
    Vector3 operator()(float r);
};

} // namespace Boidsish
