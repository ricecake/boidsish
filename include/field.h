#pragma once

#include <vector>

#include "entity.h"
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
	float     maxY, minY;  // For quick vertical culling
	float     radiusSq;    // Bounding radius of the patch itself
};

template <typename TEntity, typename TPatch>
void ApplyPatchInfluence(TEntity& entity, const TPatch& patch, const WendlandLUT& lut) {
	glm::vec3 delta = entity.position - patch.proxy.center;
	float     distSq = glm::dot(delta, delta);

	// 1. Broad-phase Culling
	// If entity is further than (Influence R + Patch Radius), skip entirely
	float combinedRadius = lut.R + std::sqrt(patch.proxy.radiusSq);
	if (distSq > (combinedRadius * combinedRadius))
		return;

	// 2. Far-Field Approximation
	// If the entity is far enough that the patch subtends a small angle
	if (distSq > patch.proxy.radiusSq * 4.0f) {
		entity.forceAccumulator += lut.Sample(delta, distSq, patch.proxy.totalNormal);
	}
	// 3. Near-Field (High Precision)
	else {
		for (size_t i = 0; i < patch.vertices.size(); ++i) {
			glm::vec3 r_vec = entity.position - patch.vertices[i];
			float     r2 = glm::dot(r_vec, r_vec);
			if (r2 < lut.R * lut.R) {
				entity.forceAccumulator += lut.Sample(r_vec, r2, patch.normals[i]);
			}
		}
	}
}

// The Generic "Driver"
// TKernelPolicy: The physics rule (Gravity, Flow, Magnetism)
// TSourceIter: Iterator for your data (Vertex array, Proxy list)
template <typename TKernelPolicy, typename TSourceIter>
glm::vec3 CalculateField(const glm::vec3& samplePos, TSourceIter begin, TSourceIter end, const TKernelPolicy& policy) {
	glm::vec3 totalField(0.0f);

	for (auto it = begin; it != end; ++it) {
		const auto& source = *it; // Your vertex or proxy

		glm::vec3 r_vec = samplePos - source.position;
		float     r2 = glm::dot(r_vec, r_vec);

		// Optimization: The generic loop handles the cutoff check
		if (r2 > policy.GetRadiusSq())
			continue;

		// "Hot-swappable" math happens here
		// The policy decides what data from 'source' matters
		totalField += policy.CalculateInfluence(r_vec, r2, source);
	}
	return totalField;
}

struct DivergenceFreePolicy {
	WendlandLUT lut; // Holds the pre-calculated weights
	float       radiusSq;

	DivergenceFreePolicy(float r): lut(r), radiusSq(r * r) {}

	float GetRadiusSq() const { return radiusSq; }

	// Expects a source with a 'normal' property
	template <typename TSource>
	glm::vec3 CalculateInfluence(const glm::vec3& r_vec, float r2, const TSource& source) const {
		// Uses the matrix-valued LUT logic
		return lut.Sample(r_vec, r2, source.normal);
	}
};

struct GravityPolicy {
	float G = 9.8f;
	float radiusSq;

	GravityPolicy(float r, float strength): radiusSq(r * r), G(strength) {}

	float GetRadiusSq() const { return radiusSq; }

	template <typename TSource>
	glm::vec3 CalculateInfluence(const glm::vec3& r_vec, float r2, const TSource& source) const {
		// F = G * m1 * m2 / r^2
		// We add a small epsilon to r2 to prevent division by zero
		float distFactor = source.mass / (r2 + 0.001f);

		// Taper off explicitly if not using a LUT
		float taper = 1.0f - (r2 / radiusSq);
		taper *= taper; // Square for smoothness

		// Direction is normalized r_vec (which is r_vec / sqrt(r2))
		// So we divide by sqrt(r2) again -> r_vec / (r * r^2) ...
		// Simply: return r_vec * (scalar)
		return glm::normalize(r_vec) * (-G * distFactor * taper);
	}
};

struct VortexPolicy {
	float strength;     // How fast it spins
	float coreRadiusSq; // Inside this, it spins like a solid object (safe)
	float maxRadiusSq;  // The hard cutoff

	VortexPolicy(float str, float coreR, float maxR):
		strength(str), coreRadiusSq(coreR * coreR), maxRadiusSq(maxR * maxR) {}

	float GetRadiusSq() const { return maxRadiusSq; }

	template <typename TSource>
	glm::vec3 CalculateInfluence(const glm::vec3& r_vec, float r2, const TSource& source) const {
		// 1. Calculate the Tangent Vector (The Swirl)
		// r_vec points FROM source TO bird.
		// If axis is Up (0,1,0), Cross(Up, r_vec) gives a horizontal tangent.
		glm::vec3 tangent = glm::cross(source.axis, r_vec);

		// 2. Calculate Intensity (Rankine Model)
		float scalar = 0.0f;

		if (r2 < coreRadiusSq) {
			// "Eye of the storm": Linear increase (Solid Body Rotation)
			// Prevents infinite velocity at r=0
			scalar = strength * (std::sqrt(r2) / std::sqrt(coreRadiusSq));
		} else {
			// Outer vortex: Inverse decay (1/r)
			// We adding a Taper so it hits exactly 0 at maxRadius
			float dist = std::sqrt(r2);
			float decay = 1.0f / dist;

			// Simple linear taper window
			float taper = 1.0f - (r2 / maxRadiusSq);
			taper *= taper; // Smooth quadratic falloff

			scalar = strength * decay * taper;
		}

		// Normalize tangent?
		// The length of 'tangent' is already proportional to distance 'r' * sin(theta).
		// To strictly control speed, we usually normalize tangent, then apply scalar.
		if (glm::length(tangent) > 1e-6f) {
			tangent = glm::normalize(tangent);
		}

		return tangent * scalar;
	}
};

template <typename TPolicyA, typename TPolicyB>
struct CompositePolicy {
	TPolicyA policyA;
	TPolicyB policyB;

	float GetRadiusSq() const { return std::max(policyA.GetRadiusSq(), policyB.GetRadiusSq()); }

	template <typename TSource>
	glm::vec3 CalculateInfluence(const glm::vec3& r_vec, float r2, const TSource& source) const {
		glm::vec3 result(0.0f);
		if (r2 < policyA.GetRadiusSq())
			result += policyA.CalculateInfluence(r_vec, r2, source);
		if (r2 < policyB.GetRadiusSq())
			result += policyB.CalculateInfluence(r_vec, r2, source);
		return result;
	}
};

/*
template <typename... TpolicyN>
struct UnionPolicy {

    float GetRadiusSq() const { return std::max(policyA.GetRadiusSq(), policyB.GetRadiusSq()); }

    template <typename TSource>
    glm::vec3 CalculateInfluence(const glm::vec3& r_vec, float r2, const TSource& source) const {
        glm::vec3 result(0.0f);
        if (r2 < policyA.GetRadiusSq())
            result += policyA.CalculateInfluence(r_vec, r2, source);
        if (r2 < policyB.GetRadiusSq())
            result += policyB.CalculateInfluence(r_vec, r2, source);
        return result;
    }
};
*/