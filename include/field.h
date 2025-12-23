#include <vector>

#include "entity.h"
#include "terrain.h"
#include <glm/glm.hpp>

class WendlandLUT {
public:
	struct Entry {
		float term1; // (-psi'/r + psi'')
		float term2; // (psi'' / r^2)
	};

	static constexpr int TABLE_SIZE = 512;
	Entry                table[TABLE_SIZE];
	float                invR2;
	float                R;

	WendlandLUT(float radius): R(radius) {
		invR2 = 1.0f / (radius * radius);
		float invR = 1.0f / radius;

		for (int i = 0; i < TABLE_SIZE; ++i) {
			// Map table index to r where r is [0, R]
			float q = (float)i / (float)(TABLE_SIZE - 1);
			float r = q * R;
			float t = 1.0f - q;

			if (q < 1e-6f) { // Handle singularity at r=0
				table[i] = {0.0f, 0.0f};
				continue;
			}

			// Wendland C2 derivatives wrt r
			float psi_grad = -20.0f * q * (t * t * t) * invR;
			float psi_hessian = 20.0f * (4.0f * q - 1.0f) * (t * t) * (invR * invR);

			table[i].term1 = (-psi_grad / r) + psi_hessian;
			table[i].term2 = psi_hessian / (r * r);
		}
	}

	// Fast lookup using squared distance
	glm::vec3 Sample(const glm::vec3& r_vec, float r2, const glm::vec3& normal) const {
		float q2 = r2 * invR2;
		if (q2 >= 1.0f)
			return glm::vec3(0.0f);

		// Map r2 to linear index [0, TABLE_SIZE-1]
		float        r = std::sqrt(r2); // Only one sqrt per valid neighbor
		int          idx = static_cast<int>((r / R) * (TABLE_SIZE - 1));
		const Entry& e = table[idx];

		float r_dot_n = glm::dot(r_vec, normal);
		return e.term1 * normal - (r_vec * (r_dot_n * e.term2));
	}
};

struct PatchProxy {
	glm::vec3 center;      // Average position of all vertices in patch
	glm::vec3 totalNormal; // Sum of all normals in patch
	float     maxZ, minZ;  // For quick vertical culling
	float     radiusSq;    // Bounding radius of the patch itself

	void ApplyPatchInfluence(auto& bird, const auto& patch, const WendlandLUT& lut) {
		glm::vec3 delta = bird.position - patch.proxy.center;
		float     distSq = glm::dot(delta, delta);

		// 1. Broad-phase Culling
		// If bird is further than (Influence R + Patch Radius), skip entirely
		float combinedRadius = lut.R + std::sqrt(patch.proxy.radiusSq);
		if (distSq > (combinedRadius * combinedRadius))
			return;

		// 2. Far-Field Approximation
		// If the bird is far enough that the patch subtends a small angle
		if (distSq > patch.proxy.radiusSq * 4.0f) {
			bird.forceAccumulator += lut.Sample(delta, distSq, patch.proxy.totalNormal);
		}
		// 3. Near-Field (High Precision)
		else {
			for (size_t i = 0; i < patch.vertices.size(); ++i) {
				glm::vec3 r_vec = bird.position - patch.vertices[i];
				float     r2 = glm::dot(r_vec, r_vec);
				if (r2 < lut.R * lut.R) {
					bird.forceAccumulator += lut.Sample(r_vec, r2, patch.normals[i]);
				}
			}
		}
	}
};
