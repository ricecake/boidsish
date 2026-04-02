#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <limits>
#include "terrain_generator.h"
#include "biome_properties.h"

using namespace Boidsish;

struct Stats {
    float min = std::numeric_limits<float>::max();
    float max = std::numeric_limits<float>::lowest();
    float mean = 0;
    float median = 0;
    float mode = 0;
    std::vector<float> values;

    void add(float v) {
        min = std::min(min, v);
        max = std::max(max, v);
        mean += v;
        values.push_back(v);
    }

    void finalize(float binSize = 1.0f) {
        if (values.empty()) return;
        mean /= values.size();
        std::sort(values.begin(), values.end());
        median = values[values.size() / 2];

        // Mode calculation using binning
        std::map<int, int> bins;
        for (float v : values) {
            bins[static_cast<int>(std::floor(v / binSize))]++;
        }
        int maxCount = 0;
        int maxBin = 0;
        for (auto const& [bin, count] : bins) {
            if (count > maxCount) {
                maxCount = count;
                maxBin = bin;
            }
        }
        mode = maxBin * binSize + binSize * 0.5f;
    }
};

int main() {
    TerrainGenerator generator(12345);
    const int size = 500;
    const float step = 4.0f;

    Stats elevationStats;
    Stats slopeStats;
    std::map<int, int> biomeCounts;

    std::cout << "Analyzing terrain..." << std::endl;

    for (int i = -size; i < size; ++i) {
        for (int j = -size; j < size; ++j) {
            float x = i * step;
            float z = j * step;

            auto [height, normal] = generator.CalculateTerrainPropertiesAtPoint(x, z);
            elevationStats.add(height);

            float slope = std::sqrt(normal.x * normal.x + normal.z * normal.z) / std::max(0.001f, std::abs(normal.y));
            slopeStats.add(slope);

            float control = generator.GetBiomeControlValue(x, z);

            float totalWeight = 0.0f;
            for (const auto& b : kBiomes) totalWeight += b.weight;
            float target = std::clamp(control, 0.0f, 1.0f) * totalWeight;

            float currentWeight = 0.0f;
            int biomeIdx = 0;
            for (size_t b = 0; b < kBiomes.size(); ++b) {
                currentWeight += kBiomes[b].weight;
                if (target <= currentWeight) {
                    biomeIdx = b;
                    break;
                }
            }
            biomeCounts[biomeIdx]++;
        }
    }

    elevationStats.finalize(5.0f); // 5 unit bins for elevation mode
    slopeStats.finalize(0.05f);   // 0.05 ratio bins for slope mode

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\n--- Elevation Stats ---\n";
    std::cout << "Min:    " << elevationStats.min << "\n";
    std::cout << "Max:    " << elevationStats.max << "\n";
    std::cout << "Mean:   " << elevationStats.mean << "\n";
    std::cout << "Median: " << elevationStats.median << "\n";
    std::cout << "Mode:   " << elevationStats.mode << " (approx)\n";

    std::cout << "\n--- Slope Stats ---\n";
    std::cout << "Min:    " << slopeStats.min << "\n";
    std::cout << "Max:    " << slopeStats.max << "\n";
    std::cout << "Mean:   " << slopeStats.mean << "\n";
    std::cout << "Median: " << slopeStats.median << "\n";
    std::cout << "Mode:   " << slopeStats.mode << " (approx)\n";

    std::cout << "\n--- Biome Breakdown ---\n";
    int totalSamples = (2 * size) * (2 * size);
    for (auto const& [idx, count] : biomeCounts) {
        std::cout << "Biome " << idx << ": " << (100.0f * count / totalSamples) << "%\n";
    }

    return 0;
}
