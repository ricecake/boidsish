#include "light.h"
#include <iostream>
#include <cstddef>

using namespace Boidsish;

int main() {
    std::cout << "sizeof(LightingUbo): " << sizeof(LightingUbo) << std::endl;
    std::cout << "offsetof(view): " << offsetof(LightingUbo, view) << std::endl;
    std::cout << "offsetof(projection): " << offsetof(LightingUbo, projection) << std::endl;
    std::cout << "offsetof(sh_coeffs): " << offsetof(LightingUbo, sh_coeffs) << std::endl;
    return 0;
}
