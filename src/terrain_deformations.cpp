#include "terrain_deformations.h"

#include <algorithm>
#include <cmath>

#include "shape.h"
#include "terrain_generator_interface.h"
#include <glm/gtc/constants.hpp>

namespace Boidsish {

	// ==================== CraterDeformation ====================

	CraterDeformation::CraterDeformation(
		uint32_t         id,
		const glm::vec3& center,
		float            radius,
		float            depth,
		float            irregularity,
		float            rim_height,
		uint32_t         seed
	):
		TerrainDeformation(id),
		center_(center),
		radius_(radius),
		depth_(depth),
		irregularity_(std::clamp(irregularity, 0.0f, 1.0f)),
		rim_height_(rim_height),
		rim_width_(radius * 0.2f), // Rim extends 20% beyond crater radius
		seed_(seed) {
		// Precompute irregularity samples
		std::mt19937                          rng(seed_);
		std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

		for (int i = 0; i < kIrregularitySamples; ++i) {
			irregularity_samples_[i] = dist(rng);
		}
	}

	void CraterDeformation::GetBounds(glm::vec3& out_min, glm::vec3& out_max) const {
		float total_radius = radius_ + rim_width_;
		out_min = center_ - glm::vec3(total_radius, depth_, total_radius);
		out_max = center_ + glm::vec3(total_radius, rim_height_, total_radius);
	}

	bool CraterDeformation::ContainsPoint(const glm::vec3& world_pos) const {
		float dx = world_pos.x - center_.x;
		float dz = world_pos.z - center_.z;
		float dist_sq = dx * dx + dz * dz;
		float max_radius = radius_ + rim_width_;
		return dist_sq <= max_radius * max_radius;
	}

	bool CraterDeformation::ContainsPointXZ(float x, float z) const {
		float dx = x - center_.x;
		float dz = z - center_.z;
		float dist_sq = dx * dx + dz * dz;
		float max_radius = radius_ + rim_width_;
		return dist_sq <= max_radius * max_radius;
	}

	float CraterDeformation::GetIrregularityOffset(float angle) const {
		if (irregularity_ <= 0.0f) {
			return 0.0f;
		}

		// Interpolate between precomputed samples
		float normalized = angle / (2.0f * 3.14159265f);
		if (normalized < 0.0f)
			normalized += 1.0f;

		float sample_pos = normalized * kIrregularitySamples;
		int   idx0 = static_cast<int>(sample_pos) % kIrregularitySamples;
		int   idx1 = (idx0 + 1) % kIrregularitySamples;
		float t = sample_pos - std::floor(sample_pos);

		float interpolated = irregularity_samples_[idx0] * (1.0f - t) + irregularity_samples_[idx1] * t;
		return interpolated * irregularity_ * radius_ * 0.3f;
	}

	float CraterDeformation::ComputeCraterProfile(float normalized_dist) const {
		// Crater profile: smooth bowl shape with optional rim
		// normalized_dist: 0 = center, 1 = rim edge, >1 = outside crater

		if (normalized_dist > 1.0f + rim_width_ / radius_) {
			return 0.0f; // Outside influence
		}

		float height_delta = 0.0f;

		if (normalized_dist <= 1.0f) {
			// Inside crater - bowl shape using cosine falloff
			// depth_ is positive, so we negate to go down
			float profile = 0.5f * (1.0f - std::cos(normalized_dist * 3.14159265f));
			height_delta = -depth_ * (1.0f - profile);
		}

		// Rim (raised edge just outside crater)
		if (rim_height_ > 0.0f && normalized_dist > 0.8f) {
			float rim_start = 0.8f;
			float rim_end = 1.0f + rim_width_ / radius_;
			float rim_center = 1.0f;

			if (normalized_dist <= rim_center) {
				// Rising to rim
				float t = (normalized_dist - rim_start) / (rim_center - rim_start);
				height_delta += rim_height_ * std::sin(t * 3.14159265f * 0.5f);
			} else if (normalized_dist <= rim_end) {
				// Falling from rim
				float t = (normalized_dist - rim_center) / (rim_end - rim_center);
				height_delta += rim_height_ * std::cos(t * 3.14159265f * 0.5f);
			}
		}

		return height_delta;
	}

	float CraterDeformation::ComputeHeightDelta(float x, float z, float current_height) const {
		(void)current_height; // Not needed for crater

		float dx = x - center_.x;
		float dz = z - center_.z;
		float dist = std::sqrt(dx * dx + dz * dz);

		// Apply irregularity
		float angle = std::atan2(dz, dx);
		float irregularity_offset = GetIrregularityOffset(angle);
		float effective_radius = radius_ + irregularity_offset;

		if (effective_radius <= 0.0f) {
			effective_radius = 0.1f;
		}

		float normalized_dist = dist / effective_radius;
		return ComputeCraterProfile(normalized_dist);
	}

	glm::vec3 CraterDeformation::TransformNormal(float x, float z, const glm::vec3& original_normal) const {
		float dx = x - center_.x;
		float dz = z - center_.z;
		float dist = std::sqrt(dx * dx + dz * dz);

		if (dist < 0.001f) {
			return original_normal; // At center, no tilt
		}

		float angle = std::atan2(dz, dx);
		float irregularity_offset = GetIrregularityOffset(angle);
		float effective_radius = radius_ + irregularity_offset;
		if (effective_radius <= 0.0f)
			effective_radius = 0.1f;

		float normalized_dist = dist / effective_radius;

		if (normalized_dist > 1.0f + rim_width_ / radius_) {
			return original_normal;
		}

		// Compute slope of crater profile
		float slope = 0.0f;
		if (normalized_dist <= 1.0f) {
			// Derivative of the bowl profile
			slope = -depth_ * 0.5f * 3.14159265f * std::sin(normalized_dist * 3.14159265f) / effective_radius;
		}

		// Direction from center
		glm::vec2 dir_xz = glm::normalize(glm::vec2(dx, dz));

		// Construct tilted normal
		// Slope is the tangent of the angle, so we tilt the normal inward
		glm::vec3 tangent_radial = glm::vec3(dir_xz.x, 0.0f, dir_xz.y);
		glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

		// Blend based on slope magnitude
		float     slope_factor = std::clamp(std::abs(slope), 0.0f, 1.0f);
		glm::vec3 tilted = glm::normalize(up - tangent_radial * slope);

		// Blend with original normal
		return glm::normalize(glm::mix(original_normal, tilted, slope_factor));
	}

	DeformationResult CraterDeformation::ComputeDeformation(
		float            x,
		float            z,
		float            current_height,
		const glm::vec3& current_normal
	) const {
		DeformationResult result;

		float dx = x - center_.x;
		float dz = z - center_.z;
		float dist = std::sqrt(dx * dx + dz * dz);

		float angle = std::atan2(dz, dx);
		float irregularity_offset = GetIrregularityOffset(angle);
		float effective_radius = radius_ + irregularity_offset;
		if (effective_radius <= 0.0f)
			effective_radius = 0.1f;

		float max_radius = effective_radius + rim_width_;

		if (dist > max_radius) {
			return result;
		}

		result.applies = true;
		result.height_delta = ComputeHeightDelta(x, z, current_height);

		// Blend weight based on distance from center
		float normalized_dist = dist / max_radius;
		result.blend_weight = 1.0f - std::pow(normalized_dist, 2.0f);

		return result;
	}

	DeformationDescriptor CraterDeformation::GetDescriptor() const {
		DeformationDescriptor desc;
		desc.type_name = GetTypeName();
		desc.center = center_;
		desc.dimensions = glm::vec3(radius_, depth_, rim_width_);
		desc.parameters = glm::vec4(irregularity_, rim_height_, 0.0f, 0.0f);
		desc.seed = seed_;
		desc.intensity = 1.0f;
		desc.deformation_type = DeformationType::Subtractive;
		return desc;
	}

	// ==================== FlattenSquareDeformation ====================

	FlattenSquareDeformation::FlattenSquareDeformation(
		uint32_t         id,
		const glm::vec3& center,
		float            half_width,
		float            half_depth,
		float            blend_distance,
		float            rotation_y
	):
		TerrainDeformation(id),
		center_(center),
		half_width_(half_width),
		half_depth_(half_depth),
		blend_distance_(blend_distance),
		rotation_y_(rotation_y) {
		cos_rot_ = std::cos(rotation_y_);
		sin_rot_ = std::sin(rotation_y_);
	}

	void FlattenSquareDeformation::GetBounds(glm::vec3& out_min, glm::vec3& out_max) const {
		// For rotated box, compute the AABB that contains it
		float total_width = half_width_ + blend_distance_;
		float total_depth = half_depth_ + blend_distance_;

		// Corners of the rotated rectangle
		glm::vec2 corners[4] = {
			{-total_width, -total_depth},
			{total_width, -total_depth},
			{total_width, total_depth},
			{-total_width, total_depth}
		};

		float min_x = std::numeric_limits<float>::max();
		float max_x = std::numeric_limits<float>::lowest();
		float min_z = std::numeric_limits<float>::max();
		float max_z = std::numeric_limits<float>::lowest();

		for (const auto& corner : corners) {
			float rotated_x = corner.x * cos_rot_ - corner.y * sin_rot_;
			float rotated_z = corner.x * sin_rot_ + corner.y * cos_rot_;

			min_x = std::min(min_x, rotated_x);
			max_x = std::max(max_x, rotated_x);
			min_z = std::min(min_z, rotated_z);
			max_z = std::max(max_z, rotated_z);
		}

		out_min = center_ + glm::vec3(min_x, -100.0f, min_z); // Large Y range
		out_max = center_ + glm::vec3(max_x, 100.0f, max_z);
	}

	float FlattenSquareDeformation::GetMaxRadius() const {
		float total_width = half_width_ + blend_distance_;
		float total_depth = half_depth_ + blend_distance_;
		return std::sqrt(total_width * total_width + total_depth * total_depth);
	}

	glm::vec2 FlattenSquareDeformation::WorldToLocal(float x, float z) const {
		// Translate to center
		float dx = x - center_.x;
		float dz = z - center_.z;

		// Rotate by -rotation_y (inverse rotation)
		float local_x = dx * cos_rot_ + dz * sin_rot_;
		float local_z = -dx * sin_rot_ + dz * cos_rot_;

		return glm::vec2(local_x, local_z);
	}

	bool FlattenSquareDeformation::ContainsPoint(const glm::vec3& world_pos) const {
		return ContainsPointXZ(world_pos.x, world_pos.z);
	}

	bool FlattenSquareDeformation::ContainsPointXZ(float x, float z) const {
		glm::vec2 local = WorldToLocal(x, z);
		float     total_width = half_width_ + blend_distance_;
		float     total_depth = half_depth_ + blend_distance_;

		return std::abs(local.x) <= total_width && std::abs(local.y) <= total_depth;
	}

	float FlattenSquareDeformation::ComputeBlendWeight(float local_x, float local_z) const {
		if (blend_distance_ <= 0.0f) {
			// Hard edge - full strength inside
			if (std::abs(local_x) <= half_width_ && std::abs(local_z) <= half_depth_) {
				return 1.0f;
			}
			return 0.0f;
		}

		// Distance from inner edge
		float dist_x = std::max(0.0f, std::abs(local_x) - half_width_);
		float dist_z = std::max(0.0f, std::abs(local_z) - half_depth_);

		if (dist_x > blend_distance_ || dist_z > blend_distance_) {
			return 0.0f;
		}

		// Smooth falloff using smoothstep
		float blend_x = (blend_distance_ > 0.0f) ? 1.0f - (dist_x / blend_distance_) : 1.0f;
		float blend_z = (blend_distance_ > 0.0f) ? 1.0f - (dist_z / blend_distance_) : 1.0f;

		// Smoothstep for smoother transition
		auto smoothstep = [](float t) { return t * t * (3.0f - 2.0f * t); };

		return smoothstep(blend_x) * smoothstep(blend_z);
	}

	float FlattenSquareDeformation::ComputeHeightDelta(float x, float z, float current_height) const {
		glm::vec2 local = WorldToLocal(x, z);
		float     blend = ComputeBlendWeight(local.x, local.y);

		if (blend <= 0.0f) {
			return 0.0f;
		}

		// Target height is center_.y
		float height_diff = center_.y - current_height;
		return height_diff * blend;
	}

	glm::vec3 FlattenSquareDeformation::TransformNormal(float x, float z, const glm::vec3& original_normal) const {
		glm::vec2 local = WorldToLocal(x, z);
		float     blend = ComputeBlendWeight(local.x, local.y);

		if (blend <= 0.0f) {
			return original_normal;
		}

		// Inside the flat area, normal should be straight up
		glm::vec3 flat_normal = glm::vec3(0.0f, 1.0f, 0.0f);

		// In blend zone, interpolate between original and flat
		return glm::normalize(glm::mix(original_normal, flat_normal, blend));
	}

	DeformationResult FlattenSquareDeformation::ComputeDeformation(
		float            x,
		float            z,
		float            current_height,
		const glm::vec3& current_normal
	) const {
		DeformationResult result;

		glm::vec2 local = WorldToLocal(x, z);
		float     blend = ComputeBlendWeight(local.x, local.y);

		if (blend <= 0.0f) {
			return result;
		}

		result.applies = true;
		result.height_delta = ComputeHeightDelta(x, z, current_height);
		result.blend_weight = blend;

		return result;
	}

	DeformationDescriptor FlattenSquareDeformation::GetDescriptor() const {
		DeformationDescriptor desc;
		desc.type_name = GetTypeName();
		desc.center = center_;
		desc.dimensions = glm::vec3(half_width_, half_depth_, blend_distance_);
		desc.parameters = glm::vec4(rotation_y_, 0.0f, 0.0f, 0.0f);
		desc.seed = 0;
		desc.intensity = 1.0f;
		desc.deformation_type = DeformationType::Subtractive; // Actually context-dependent
		return desc;
	}

	// ==================== AkiraDeformation ====================

	AkiraDeformation::AkiraDeformation(uint32_t id, const glm::vec3& center, float radius):
		TerrainDeformation(id), center_(center), radius_(radius) {
		// For the bottom 1/3 of a sphere:
		// h = 2R/3
		// a = radius = R * sqrt(8)/3
		// R = 3 * radius / sqrt(8)
		sphere_radius_ = (3.0f * radius_) / std::sqrt(8.0f);
		depth_ = (2.0f * sphere_radius_) / 3.0f;
	}

	void AkiraDeformation::GetBounds(glm::vec3& out_min, glm::vec3& out_max) const {
		out_min = center_ - glm::vec3(radius_, depth_, radius_);
		out_max = center_ + glm::vec3(radius_, 0.0f, radius_);
	}

	bool AkiraDeformation::ContainsPoint(const glm::vec3& world_pos) const {
		return ContainsPointXZ(world_pos.x, world_pos.z);
	}

	bool AkiraDeformation::ContainsPointXZ(float x, float z) const {
		float dx = x - center_.x;
		float dz = z - center_.z;
		return (dx * dx + dz * dz) <= (radius_ * radius_);
	}

	float AkiraDeformation::ComputeHeightDelta(float x, float z, float current_height) const {
		(void)current_height;
		float dx = x - center_.x;
		float dz = z - center_.z;
		float dist_sq = dx * dx + dz * dz;

		if (dist_sq > radius_ * radius_) {
			return 0.0f;
		}

		// Sphere equation: (x-xc)^2 + (y-yc)^2 + (z-zc)^2 = R^2
		// y = yc - sqrt(R^2 - dx^2 - dz^2)
		// yc is sphere_radius_ / 3.0f above the terrain cut level
		float yc = center_.y + sphere_radius_ / 3.0f;
		float sphere_y = yc - std::sqrt(std::max(0.0f, sphere_radius_ * sphere_radius_ - dist_sq));

		return sphere_y - center_.y;
	}

	glm::vec3 AkiraDeformation::TransformNormal(float x, float z, const glm::vec3& original_normal) const {
		float dx = x - center_.x;
		float dz = z - center_.z;
		float dist_sq = dx * dx + dz * dz;

		if (dist_sq > radius_ * radius_ || dist_sq < 0.0001f) {
			return original_normal;
		}

		// Normal of a sphere (x-xc, y-yc, z-zc) / R
		// For the bottom surface, the normal points generally upwards/inwards
		float yc = center_.y + sphere_radius_ / 3.0f;
		float sphere_y = yc - std::sqrt(std::max(0.0f, sphere_radius_ * sphere_radius_ - dist_sq));

		glm::vec3 sphere_normal = glm::normalize(glm::vec3(dx, sphere_y - yc, dz));
		// We want the normal to point away from the sphere center (outwards from the bowl)
		// The bottom surface normal should have a positive Y component.
		if (sphere_normal.y < 0)
			sphere_normal = -sphere_normal;

		return sphere_normal;
	}

	DeformationResult AkiraDeformation::ComputeDeformation(
		float            x,
		float            z,
		float            current_height,
		const glm::vec3& current_normal
	) const {
		DeformationResult result;
		if (!ContainsPointXZ(x, z)) {
			return result;
		}

		result.applies = true;
		result.height_delta = ComputeHeightDelta(x, z, current_height);
		// For Akira, we don't want smooth blending at the edges, so weight is always 1.0 if inside
		result.blend_weight = 1.0f;

		return result;
	}

	DeformationDescriptor AkiraDeformation::GetDescriptor() const {
		DeformationDescriptor desc;
		desc.type_name = GetTypeName();
		desc.center = center_;
		desc.dimensions = glm::vec3(radius_, sphere_radius_, depth_);
		desc.parameters = glm::vec4(0.0f);
		desc.seed = 0;
		desc.intensity = 1.0f;
		desc.deformation_type = DeformationType::Subtractive;
		return desc;
	}

	// ==================== CylinderHoleDeformation ====================

	CylinderHoleDeformation::CylinderHoleDeformation(uint32_t id, const glm::vec3& center, float radius, float length, const glm::quat& orientation, bool open_ended)
		: TerrainDeformation(id), center_(center), radius_(radius), length_(length), orientation_(orientation), open_ended_(open_ended) {
	}

	void CylinderHoleDeformation::GetBounds(glm::vec3& out_min, glm::vec3& out_max) const {
		// Compute AABB of the oriented cylinder
		float r = radius_;
		float h = length_ * 0.5f;

		glm::vec3 corners[8] = {
			{-r, -h, -r}, {r, -h, -r}, {r, -h, r}, {-r, -h, r},
			{-r, h, -r}, {r, h, -r}, {r, h, r}, {-r, h, r}
		};

		out_min = glm::vec3(std::numeric_limits<float>::max());
		out_max = glm::vec3(std::numeric_limits<float>::lowest());

		for (int i = 0; i < 8; ++i) {
			glm::vec3 world_corner = center_ + orientation_ * corners[i];
			out_min = glm::min(out_min, world_corner);
			out_max = glm::max(out_max, world_corner);
		}

		// Add some padding
		out_min -= glm::vec3(1.0f);
		out_max += glm::vec3(1.0f);
	}

	bool CylinderHoleDeformation::ContainsPoint(const glm::vec3& world_pos) const {
		glm::vec3 local_p = glm::inverse(orientation_) * (world_pos - center_);

		// In local space, cylinder is along Y axis, from -length/2 to length/2
		if (std::abs(local_p.y) > length_ * 0.5f) return false;

		float dist_sq = local_p.x * local_p.x + local_p.z * local_p.z;
		return dist_sq <= radius_ * radius_;
	}

	bool CylinderHoleDeformation::ContainsPointXZ(float x, float z) const {
		// For oriented cylinder, a simple XZ check is not enough because the hole
		// depends on the terrain height at that point.
		// However, we can check if any part of the cylinder column at (x, z) could
		// overlap the cylinder volume.

		// Project the cylinder's world-space AABB to XZ
		glm::vec3 min_b, max_b;
		GetBounds(min_b, max_b);
		if (x < min_b.x || x > max_b.x || z < min_b.z || z > max_b.z) return false;

		return true;
	}

	float CylinderHoleDeformation::ComputeHeightDelta(float x, float z, float current_height) const {
		(void)x; (void)z; (void)current_height;
		return 0.0f;
	}

	bool CylinderHoleDeformation::IsHole(float x, float z, float current_height) const {
		return ContainsPoint(glm::vec3(x, current_height, z));
	}

	glm::vec3 CylinderHoleDeformation::TransformNormal(float x, float z, const glm::vec3& original_normal) const {
		(void)x; (void)z;
		return original_normal;
	}

	DeformationResult CylinderHoleDeformation::ComputeDeformation(float x, float z, float current_height, const glm::vec3& current_normal) const {
		DeformationResult result;
		if (!ContainsPointXZ(x, z)) return result;

		if (IsHole(x, z, current_height)) {
			result.applies = true;
			result.is_hole = true;
			result.blend_weight = 1.0f;
		}
		return result;
	}

	DeformationDescriptor CylinderHoleDeformation::GetDescriptor() const {
		DeformationDescriptor desc;
		desc.type_name = GetTypeName();
		desc.center = center_;
		desc.dimensions = glm::vec3(radius_, length_, open_ended_ ? 1.0f : 0.0f);
		desc.parameters = glm::vec4(orientation_.x, orientation_.y, orientation_.z, orientation_.w);
		desc.deformation_type = DeformationType::Subtractive;
		return desc;
	}

	void CylinderHoleDeformation::GenerateMesh(const ITerrainGenerator& terrain) {
		const int SAMPLES = 32;
		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;

		// We will build a standard cylinder mesh in local space and transform it.
		// Local axis is Y. Bottom cap at -length/2, Top cap at length/2.
		float h2 = length_ * 0.5f;

		// 1. Cylinder sides
		for (int i = 0; i <= SAMPLES; ++i) {
			float t = (float)i / SAMPLES;
			float angle = t * 2.0f * glm::pi<float>();
			float cosA = cos(angle);
			float sinA = sin(angle);

			glm::vec3 normal(cosA, 0, sinA);

			// Bottom vertex
			Vertex v_bot;
			v_bot.Position = glm::vec3(radius_ * cosA, -h2, radius_ * sinA);
			v_bot.Normal = normal;
			v_bot.TexCoords = glm::vec2(t, 0.0f);
			vertices.push_back(v_bot);

			// Top vertex
			Vertex v_top;
			v_top.Position = glm::vec3(radius_ * cosA, h2, radius_ * sinA);
			v_top.Normal = normal;
			v_top.TexCoords = glm::vec2(t, 1.0f);
			vertices.push_back(v_top);
		}

		for (int i = 0; i < SAMPLES; ++i) {
			int b0 = i * 2;
			int t0 = i * 2 + 1;
			int b1 = (i + 1) * 2;
			int t1 = (i + 1) * 2 + 1;

			// CCW winding
			indices.push_back(b0);
			indices.push_back(b1);
			indices.push_back(t0);

			indices.push_back(t0);
			indices.push_back(b1);
			indices.push_back(t1);
		}

		// 2. Caps
		int cap_start_idx = static_cast<int>(vertices.size());

		// Bottom cap center
		Vertex v_bot_center;
		v_bot_center.Position = glm::vec3(0, -h2, 0);
		v_bot_center.Normal = glm::vec3(0, -1, 0);
		v_bot_center.TexCoords = glm::vec2(0.5f, 0.5f);
		vertices.push_back(v_bot_center);

		// Top cap center
		Vertex v_top_center;
		v_top_center.Position = glm::vec3(0, h2, 0);
		v_top_center.Normal = glm::vec3(0, 1, 0);
		v_top_center.TexCoords = glm::vec2(0.5f, 0.5f);
		vertices.push_back(v_top_center);

		int bot_center_idx = cap_start_idx;
		int top_center_idx = cap_start_idx + 1;

		for (int i = 0; i < SAMPLES; ++i) {
			float t = (float)i / SAMPLES;
			float angle = t * 2.0f * glm::pi<float>();
			float cosA = cos(angle);
			float sinA = sin(angle);

			// Bottom cap ring
			Vertex v_bot;
			v_bot.Position = glm::vec3(radius_ * cosA, -h2, radius_ * sinA);
			v_bot.Normal = glm::vec3(0, -1, 0);
			v_bot.TexCoords = glm::vec2(cosA * 0.5f + 0.5f, sinA * 0.5f + 0.5f);
			vertices.push_back(v_bot);

			// Top cap ring
			Vertex v_top;
			v_top.Position = glm::vec3(radius_ * cosA, h2, radius_ * sinA);
			v_top.Normal = glm::vec3(0, 1, 0);
			v_top.TexCoords = glm::vec2(cosA * 0.5f + 0.5f, sinA * 0.5f + 0.5f);
			vertices.push_back(v_top);
		}

		int ring_start = cap_start_idx + 2;
		if (!open_ended_) {
			for (int i = 0; i < SAMPLES; ++i) {
				int i0 = ring_start + i * 2;
				int i1 = ring_start + ((i + 1) % SAMPLES) * 2;

				// Bottom cap (facing down, so CW from above is CCW from below)
				indices.push_back(bot_center_idx);
				indices.push_back(i1);
				indices.push_back(i0);

				// Top cap
				indices.push_back(top_center_idx);
				indices.push_back(i0 + 1);
				indices.push_back(i1 + 1);
			}
		}

		// Transform all vertices to world space
		for (auto& v : vertices) {
			v.Position = center_ + orientation_ * v.Position;
			v.Normal = orientation_ * v.Normal;
		}

		interior_mesh_ = std::make_shared<CustomMeshShape>(vertices, indices);
		interior_mesh_->SetColor(0.35f, 0.25f, 0.18f); // Soil/Rock color
		interior_mesh_->SetUsePBR(true);
		interior_mesh_->SetRoughness(0.9f);
		interior_mesh_->SetMetallic(0.0f);
	}

} // namespace Boidsish
