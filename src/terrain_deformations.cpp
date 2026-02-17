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

	CylinderHoleDeformation::CylinderHoleDeformation(uint32_t id, const glm::vec3& center, float radius, float depth)
		: TerrainDeformation(id), center_(center), radius_(radius), depth_(depth) {
	}

	void CylinderHoleDeformation::GetBounds(glm::vec3& out_min, glm::vec3& out_max) const {
		out_min = center_ - glm::vec3(radius_, depth_ + 10.0f, radius_); // extra buffer for mesh floor
		out_max = center_ + glm::vec3(radius_, 100.0f, radius_);
	}

	bool CylinderHoleDeformation::ContainsPoint(const glm::vec3& world_pos) const {
		return ContainsPointXZ(world_pos.x, world_pos.z);
	}

	bool CylinderHoleDeformation::ContainsPointXZ(float x, float z) const {
		float dx = x - center_.x;
		float dz = z - center_.z;
		return (dx * dx + dz * dz) <= (radius_ * radius_);
	}

	float CylinderHoleDeformation::ComputeHeightDelta(float x, float z, float current_height) const {
		(void)x; (void)z; (void)current_height;
		return 0.0f;
	}

	bool CylinderHoleDeformation::IsHole(float x, float z, float current_height) const {
		return ContainsPointXZ(x, z);
	}

	glm::vec3 CylinderHoleDeformation::TransformNormal(float x, float z, const glm::vec3& original_normal) const {
		(void)x; (void)z;
		return original_normal;
	}

	DeformationResult CylinderHoleDeformation::ComputeDeformation(float x, float z, float current_height, const glm::vec3& current_normal) const {
		DeformationResult result;
		if (!ContainsPointXZ(x, z)) return result;

		result.applies = true;
		result.is_hole = true;
		result.blend_weight = 1.0f;
		return result;
	}

	DeformationDescriptor CylinderHoleDeformation::GetDescriptor() const {
		DeformationDescriptor desc;
		desc.type_name = GetTypeName();
		desc.center = center_;
		desc.dimensions = glm::vec3(radius_, depth_, 0.0f);
		desc.deformation_type = DeformationType::Subtractive;
		return desc;
	}

	void CylinderHoleDeformation::GenerateMesh(const ITerrainGenerator& terrain) {
		const int SAMPLES = 32;
		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;

		// We will build:
		// 1. A rim circle at sampled terrain heights
		// 2. A floor circle at (rim height - depth)
		// 3. A center point on the floor

		// Rim vertices (0 to SAMPLES-1)
		// Floor vertices (SAMPLES to 2*SAMPLES-1)
		// Center vertex (2*SAMPLES)

		for (int i = 0; i < SAMPLES; ++i) {
			float angle = (float)i / SAMPLES * 2.0f * glm::pi<float>();
			float dx = radius_ * cos(angle);
			float dz = radius_ * sin(angle);
			float x = center_.x + dx;
			float z = center_.z + dz;

			// Sample height at the rim
			auto [h, norm] = terrain.GetTerrainPropertiesAtPoint(x, z);

			// Rim vertex
			Vertex v_rim;
			v_rim.Position = glm::vec3(x, h, z);
			// Normal points inward for the cylinder wall
			v_rim.Normal = glm::normalize(glm::vec3(-dx, 0, -dz));
			v_rim.TexCoords = glm::vec2((float)i / SAMPLES, 1.0f);
			vertices.push_back(v_rim);
		}

		for (int i = 0; i < SAMPLES; ++i) {
			// Floor vertex (same XZ as rim, but lower)
			Vertex v_floor;
			v_floor.Position = vertices[i].Position - glm::vec3(0, depth_, 0);
			v_floor.Normal = glm::vec3(0, 1, 0); // Floor points up
			v_floor.TexCoords = glm::vec2((float)i / SAMPLES, 0.0f);
			vertices.push_back(v_floor);
		}

		// Floor center
		Vertex v_center;
		v_center.Position = center_;
		// Determine average rim height for center
		float avg_h = 0;
		for (int i = 0; i < SAMPLES; ++i) avg_h += vertices[i].Position.y;
		avg_h /= SAMPLES;
		v_center.Position.y = avg_h - depth_;
		v_center.Normal = glm::vec3(0, 1, 0);
		v_center.TexCoords = glm::vec2(0.5f, 0.5f);
		vertices.push_back(v_center);
		unsigned int center_idx = 2 * SAMPLES;

		// Indices
		for (int i = 0; i < SAMPLES; ++i) {
			int next = (i + 1) % SAMPLES;

			// Wall triangles
			// Rim[i], Floor[i], Rim[next]
			indices.push_back(i);
			indices.push_back(i + SAMPLES);
			indices.push_back(next);

			// Floor[i], Floor[next], Rim[next]
			indices.push_back(i + SAMPLES);
			indices.push_back(next + SAMPLES);
			indices.push_back(next);

			// Floor triangles (connecting to center) - CCW winding from above
			indices.push_back(i + SAMPLES);
			indices.push_back(next + SAMPLES);
			indices.push_back(center_idx);
		}

		interior_mesh_ = std::make_shared<CustomMeshShape>(vertices, indices);
		interior_mesh_->SetColor(0.35f, 0.25f, 0.18f); // Soil/Rock color
		interior_mesh_->SetUsePBR(true);
		interior_mesh_->SetRoughness(0.9f);
		interior_mesh_->SetMetallic(0.0f);
	}

} // namespace Boidsish
