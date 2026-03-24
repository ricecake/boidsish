#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "biome_properties.h"
#include "frustum.h"
#include "model.h"
#include "procedural_generator.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <shader.h>

namespace Boidsish {

	class ITerrainGenerator;
	class TerrainRenderManager;
	struct Camera;

	// Properties for configuring decor placement and appearance
	struct DecorProperties {
		float     min_density = 0.1f;              // Minimum density (ensures all visible areas get some decor)
		float     max_density = 1.0f;              // Maximum density at close range
		float     base_scale = 1.0f;               // Base scale factor
		float     scale_variance = 0.2f;           // Random scale variation (+/-)
		float     min_height = -100.0f;            // Minimum terrain height for placement
		float     max_height = 1000.0f;            // Maximum terrain height for placement
		float     min_slope = 0.0f;                // Minimum terrain slope (0=flat)
		float     max_slope = 1.0f;                // Maximum terrain slope (1=vertical)
		glm::vec3 base_rotation = glm::vec3(0.0f); // Base rotation in degrees (pitch, yaw, roll)
		bool      random_yaw = true;               // Apply random Y rotation
		bool      align_to_terrain =
			false;          // If true, align to terrain normal (bush on cliff); if false, align to world up (tree)
		BiomeBitset biomes; // Bitmask of biomes where this decor can grow
		float       detail_distance = 0.0f;     // If > 0, only rendered if within this distance
		float       wind_responsiveness = 1.0f; // Controls how much the decor reacts to wind
		float       wind_rim_highlight = 0.0f;  // Rim highlight intensity when wind deflection occurs

		void SetDensity(float d) {
			min_density = d * 0.2f;
			max_density = d;
		}
	};

	// Matches std140 layout in decor_placement.comp GlobalPlacementParams UBO
	struct alignas(16) PlacementGlobalsGPU {
		glm::vec4 camera_and_scale; // xy=cameraPos, z=worldScale, w=maxTerrainHeight
		glm::vec4 distance_params;  // x=densityFalloffStart, y=densityFalloffEnd, z=maxDecorDistance, w=maxInstances
	};

	// Matches std430 layout in decor_placement.comp ChunkParams SSBO
	struct ChunkParamsGPU {
		glm::vec4  offset_slice_size; // xy=worldOffset, z=slice, w=chunkSize
		glm::ivec4 indices;           // x=baseInstanceIndex, yzw=unused
	};

	// Matches std140 layout in decor_placement.comp DecorTypeParams UBO
	struct alignas(16) DecorTypeGPU {
		glm::vec4  density_scale; // x=minDensity, y=maxDensity, z=baseScale, w=scaleVariance
		glm::vec4  height_slope;  // x=minHeight, y=maxHeight, z=minSlope, w=maxSlope
		glm::vec4  rotation;      // xyz=baseRotation (radians), w=detailDistance
		glm::vec4  aabb_min;      // xyz=model AABB min, w=unused
		glm::vec4  aabb_max;      // xyz=model AABB max, w=unused
		glm::uvec4 flags;         // x=biomeMask, y=randomYaw, z=alignToTerrain, w=typeIndex
	};

	struct DecorType {
		std::shared_ptr<Model> model;
		DecorProperties        props;

		// GPU resources
		unsigned int ssbo = 0;                   // Main storage (persistent)
		unsigned int visible_ssbo = 0;           // Culled storage (per-frame)
		unsigned int indirect_buffer = 0;        // MDI commands
		unsigned int shadow_indirect_buffer = 0; // MDI commands for shadow pass
		unsigned int count_buffer = 0;           // For culling atomic counter

		// Cached instance count (read back after compute, used during render)
		unsigned int cached_count = 0;
	};

	struct DecorInstance {
		glm::vec3 center;
		glm::vec3 scale;
		glm::quat rotation;
		AABB      aabb;
	};

	struct DecorTypeResults {
		std::string                model_path;
		std::vector<DecorInstance> instances;
	};

	class DecorManager {
	public:
		DecorManager();
		~DecorManager();

		// Simple overload for basic usage (sets max_density, min_density = max * 0.2)
		void AddDecorType(const std::string& model_path, float density);

		// Full overload with all properties
		void AddDecorType(const std::string& model_path, const DecorProperties& props);

		// Overloads for using existing Model objects
		void AddDecorType(std::shared_ptr<Model> model, float density);
		void AddDecorType(std::shared_ptr<Model> model, const DecorProperties& props);

		// Populates the manager with default decor (trees, rocks, etc.)
		// Only adds if no decor types have been added yet.
		void PopulateDefaultDecor();

		// Adds a procedurally generated decor with multiple variants
		void AddProceduralDecor(ProceduralType type, const DecorProperties& props, int variants = 3);

		// Static helpers for getting default properties
		static DecorProperties GetDefaultTreeProperties();
		static DecorProperties GetDefaultDeadTreeProperties();
		static DecorProperties GetDefaultRockProperties();

		void Update(
			float                                 delta_time,
			const Camera&                         camera,
			const Frustum&                        frustum,
			const ITerrainGenerator&              terrain_gen,
			std::shared_ptr<TerrainRenderManager> render_manager
		);

		/**
		 * @brief Prepares the resources for all decor types.
		 * This ensures models are uploaded to the megabuffer and indirect buffers are initialized.
		 */
		void PrepareResources(Megabuffer* mb);

		/**
		 * @brief Performs GPU culling for all decor types.
		 * Should be called once per pass (main camera or shadow-casting light) before Render().
		 * Results are stored in the respective indirect buffers and visible SSBOs.
		 */
		void Cull(
			const glm::mat4&                      view,
			const glm::mat4&                      projection,
			int                                   viewport_width,
			int                                   viewport_height,
			const std::optional<glm::mat4>&       light_space_matrix = std::nullopt,
			const std::optional<glm::vec3>&       light_dir = std::nullopt,
			std::shared_ptr<TerrainRenderManager> render_manager = nullptr
		);

		/**
		 * @brief Renders the already-culled decor instances.
		 * Uses the results from the most recent Cull() call.
		 */
		void Render(
			const glm::mat4&                view,
			const glm::mat4&                projection,
			const std::optional<glm::mat4>& light_space_matrix = std::nullopt,
			Shader*                         shader_override = nullptr
		);

		void SetEnabled(bool enabled) { enabled_ = enabled; }

		bool IsEnabled() const { return enabled_; }

		/// Set Hi-Z occlusion data for GPU culling. Call each frame before Render().
		void SetHiZData(GLuint hiz_texture, int hiz_width, int hiz_height, int mip_count, const glm::mat4& prev_vp) {
			hiz_texture_ = hiz_texture;
			hiz_width_ = hiz_width;
			hiz_height_ = hiz_height;
			hiz_mip_count_ = mip_count;
			hiz_prev_vp_ = prev_vp;
			hiz_enabled_ = true;
		}

		void SetHiZEnabled(bool enabled) { hiz_enabled_ = enabled; }

		// Distance at which density starts to fall off from max toward min
		void SetDensityFalloffStart(float distance) { density_falloff_start_ = distance; }

		// Distance at which density reaches minimum
		void SetDensityFalloffEnd(float distance) { density_falloff_end_ = distance; }

		// Maximum distance at which decor is placed at all
		void SetMaxDistance(float distance) { max_decor_distance_ = distance; }

		// Minimum screen-space size in pixels for culling
		void SetMinPixelSize(float size) { min_pixel_size_ = size; }

		/**
		 * @brief Retrieves all decor instances within the specified terrain chunks.
		 * This is an on-demand query that generates decor for the requested chunks
		 * if they are currently registered in the render manager.
		 *
		 * @param chunk_keys List of (x, z) chunk coordinates to query.
		 * @param render_manager The render manager providing terrain data.
		 * @param terrain_gen The terrain generator providing world scale and height.
		 * @return Vector of decor results grouped by type.
		 */
		std::vector<DecorTypeResults> GetDecorInChunks(
			const std::vector<std::pair<int, int>>& chunk_keys,
			std::shared_ptr<TerrainRenderManager>   render_manager,
			const ITerrainGenerator&                terrain_gen
		);

	private:
		void _Initialize();
		void _UpdateAllocation(
			const Camera&                         camera,
			const Frustum&                        frustum,
			const ITerrainGenerator&              terrain_gen,
			std::shared_ptr<TerrainRenderManager> render_manager
		);

		bool                           enabled_ = true;
		bool                           initialized_ = false;
		std::vector<DecorType>         decor_types_;
		std::unique_ptr<ComputeShader> placement_shader_;
		std::unique_ptr<ComputeShader> culling_shader_;
		std::unique_ptr<ComputeShader> update_commands_shader_;
		std::shared_ptr<Shader>        render_shader_;

		// Block allocation
		struct ChunkAllocation {
			int      block_index;
			uint32_t update_count = 0; // mirrors the render manager's per-chunk update_count
			bool     is_dirty;
			uint32_t last_seen_frame = 0; // frame when chunk was last present in chunk_info
		};

		std::map<std::pair<int, int>, ChunkAllocation> active_chunks_;
		std::vector<int>                               free_blocks_;
		uint32_t                                       frame_counter_ = 0;

		// Grace period: keep decor blocks alive for this many frames after their
		// terrain chunk disappears from the render manager (e.g. due to LRU eviction).
		// Prevents regeneration churn when chunks cycle in and out.
		static constexpr uint32_t kChunkGracePeriodFrames = 1;

		// Cached camera state for terrain occlusion and priority sorting
		glm::vec3 camera_pos_ = glm::vec3(0.0f);
		glm::vec2 camera_forward_2d_ = glm::vec2(0.0f, -1.0f);
		glm::vec2 camera_rotation_delta_ = glm::vec2(0.0f); // per-frame turn direction (XZ)
		glm::vec2 prev_camera_forward_2d_ = glm::vec2(0.0f, -1.0f);
		float     last_world_scale_ = 0.0f;

		// Cap on new chunks generated per frame to avoid spikes
		static constexpr int kMaxNewChunksPerFrame = 24;

		// Per-type properties UBO for placement shader (uploaded in PrepareResources)
		GLuint decor_props_ubo_ = 0;
		// Global placement params UBO and per-chunk SSBO (uploaded per dispatch frame)
		GLuint placement_globals_ubo_ = 0;
		GLuint chunk_params_ssbo_ = 0;

		// Distance-based density parameters
		float density_falloff_start_ = 200.0f;
		float density_falloff_end_ = 500.0f;
		float max_decor_distance_ = 600.0f;
		float min_pixel_size_ = 4.0f;

		// Hi-Z occlusion culling data (set per-frame by SetHiZData)
		GLuint    hiz_texture_ = 0;
		int       hiz_width_ = 0;
		int       hiz_height_ = 0;
		int       hiz_mip_count_ = 0;
		glm::mat4 hiz_prev_vp_{1.0f};
		bool      hiz_enabled_ = false;

		// Block validity SSBO: one uint per block. 1=valid (has placed decor),
		// 0=invalid (freed, stale data). Checked by cull shader to skip freed blocks
		// without needing to zero 64KB of instance data per type.
		GLuint block_validity_ssbo_ = 0;

		static constexpr int kInstancesPerChunk = 1024;
		static constexpr int kMaxActiveChunks = 1024;
		static constexpr int kMaxInstancesPerType = kInstancesPerChunk * kMaxActiveChunks; // 147,456
	};

} // namespace Boidsish
