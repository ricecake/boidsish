#include <iostream>
#include <iomanip>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "sdf_utils.h"
#include "logger.h"

using namespace Boidsish;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <sdf_file_path>" << std::endl;
        return 1;
    }

    std::string sdf_path = argv[1];
    SdfData sdf;

    logger::INFO("Loading SDF from: {}", sdf_path);
    if (!SdfUtils::LoadSDF(sdf, sdf_path)) {
        logger::ERROR("Failed to load SDF file");
        return 1;
    }

    logger::INFO("SDF Dimensions: {}x{}x{}", sdf.res.x, sdf.res.y, sdf.res.z);
    logger::INFO("SDF AABB Min: ({}, {}, {})", sdf.aabb_min.x, sdf.aabb_min.y, sdf.aabb_min.z);
    logger::INFO("SDF AABB Max: ({}, {}, {})", sdf.aabb_max.x, sdf.aabb_max.y, sdf.aabb_max.z);

    glm::vec3 center = (sdf.aabb_min + sdf.aabb_max) * 0.5f;
    glm::vec3 extent = (sdf.aabb_max - sdf.aabb_min);

    std::cout << "\n--- SDF Validation Samples ---\n";
    std::cout << std::fixed << std::setprecision(4);
    std::cout << std::left << std::setw(30) << "Location" << " | " << "Distance" << std::endl;
    std::cout << std::string(45, '-') << std::endl;

    // Sample points
    std::vector<std::pair<std::string, glm::vec3>> test_points = {
        {"Center", center},
        {"Origin", glm::vec3(0, 0, 0)},
        {"AABB Min", sdf.aabb_min},
        {"AABB Max", sdf.aabb_max},
        {"Slightly inside Min", sdf.aabb_min + extent * 0.1f},
        {"Slightly inside Max", sdf.aabb_max - extent * 0.1f},
        {"Outside (+X)", sdf.aabb_max + glm::vec3(extent.x * 0.5f, 0, 0)},
        {"Outside (-Y)", sdf.aabb_min - glm::vec3(0, extent.y * 0.5f, 0)}
    };

    // Add some random-ish points along axes
    for (int i = 1; i <= 5; ++i) {
        float f = i / 6.0f;
        test_points.push_back({"Axis X " + std::to_string(i), sdf.aabb_min + glm::vec3(extent.x * f, extent.y * 0.5f, extent.z * 0.5f)});
        test_points.push_back({"Axis Y " + std::to_string(i), sdf.aabb_min + glm::vec3(extent.x * 0.5f, extent.y * f, extent.z * 0.5f)});
    }

    int non_zero_count = 0;
    int negative_count = 0;
    int positive_count = 0;

    for (const auto& [name, pos] : test_points) {
        float dist = sdf.Sample(pos);
        std::cout << std::left << std::setw(30) << name << " | " << dist << std::endl;

        if (std::abs(dist) > 1e-5f) non_zero_count++;
        if (dist < 0) negative_count++;
        else if (dist > 0) positive_count++;
    }

    std::cout << "\n--- Statistics ---\n";
    std::cout << "Total samples: " << test_points.size() << std::endl;
    std::cout << "Non-zero distances: " << non_zero_count << std::endl;
    std::cout << "Negative (inside): " << negative_count << std::endl;
    std::cout << "Positive (outside): " << positive_count << std::endl;

    if (non_zero_count > test_points.size() / 2 && positive_count > 0) {
        logger::INFO("Validation: Plausible results detected.");
    } else {
        logger::WARNING("Validation: Results might be suspicious (too many zeros or no outside values).");
    }

    return 0;
}
