#include "decor_manager.h"

#include <algorithm>

#include "ConfigManager.h"
#include "graphics.h"
#include "logger.h"
#include "terrain_generator_interface.h"
#include "terrain_render_manager.h"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {

	DecorManager::DecorManager() {}

	DecorManager::~DecorManager() {
		for (auto& type : decor_types_) {
			if (type.instances_ssbo != 0)
				glDeleteBuffers(1, &type.instances_ssbo);
			if (type.indirect_commands_ssbo != 0)
				glDeleteBuffers(1, &type.indirect_commands_ssbo);
		}
		if (chunk_data_ssbo_ != 0)
			glDeleteBuffers(1, &chunk_data_ssbo_);
		if (chunk_status_ssbo_ != 0)
			glDeleteBuffers(1, &chunk_status_ssbo_);
	}

	void DecorManager::_Initialize() {
		if (initialized_)
			return;

		placement_shader_ = std::make_unique<ComputeShader>("shaders/decor_placement.comp");

		// Check if compute shader compiled successfully
		if (!placement_shader_->isValid()) {
			logger::ERROR("Failed to compile decor placement compute shader - decor will be disabled");
			initialized_ = true; // Mark as initialized to prevent repeated attempts
			return;
		}

		glGenBuffers(1, &chunk_data_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk_data_ssbo_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxChunks * sizeof(ChunkData), nullptr, GL_DYNAMIC_DRAW);

		glGenBuffers(1, &chunk_status_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk_status_ssbo_);
		// Store ChunkStatus per chunk
		std::vector<uint32_t> zeros(kMaxChunks * 4, 0);
		glBufferData(GL_SHADER_STORAGE_BUFFER, zeros.size() * sizeof(uint32_t), zeros.data(), GL_DYNAMIC_DRAW);

		initialized_ = true;
	}

	void DecorManager::AddDecorType(const std::string& model_path, float density) {
		DecorProperties props;
		props.SetDensity(density);
		AddDecorType(model_path, props);
	}

	void DecorManager::AddDecorType(const std::string& model_path, const DecorProperties& props) {
		_Initialize();

		// Resize status buffer to accommodate new decor type
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk_status_ssbo_);
		size_t new_size = (decor_types_.size() + 1) * kMaxChunks * sizeof(ChunkStatus);
		// Note: This is a bit inefficient to reallocate every time, but AddDecorType is rare.
		// Also glBufferData clears existing data, which is actually what we want for a NEW type.
		// To keep old data, we'd need to copy it, but since each type has its own range,
		// we only care about clearing the NEW range.

		// Let's do it properly:
		std::vector<uint8_t> old_data;
		if (!decor_types_.empty()) {
			size_t old_size = decor_types_.size() * kMaxChunks * sizeof(ChunkStatus);
			old_data.resize(old_size);
			glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, old_size, old_data.data());
		}

		glBufferData(GL_SHADER_STORAGE_BUFFER, new_size, nullptr, GL_DYNAMIC_DRAW);
		if (!old_data.empty()) {
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, old_data.size(), old_data.data());
		}
		// Clear the new portion
		std::vector<uint32_t> zeros(kMaxChunks * 4, 0);
		size_t offset = decor_types_.size() * kMaxChunks * sizeof(ChunkStatus);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, zeros.size() * sizeof(uint32_t), zeros.data());

		DecorType type;
		type.model = std::make_shared<Model>(model_path);
		type.props = props;
		type.num_meshes = static_cast<unsigned int>(type.model->getMeshes().size());

		glGenBuffers(1, &type.instances_ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, type.instances_ssbo);
		glBufferData(
			GL_SHADER_STORAGE_BUFFER,
			kMaxChunks * kMaxInstancesPerChunk * sizeof(glm::mat4),
			nullptr,
			GL_DYNAMIC_DRAW
		);

		glGenBuffers(1, &type.indirect_commands_ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, type.indirect_commands_ssbo);

		// Initialize indirect commands with 0 instances
		std::vector<DrawElementsIndirectCommand> commands(kMaxChunks * type.num_meshes);
		for (size_t m = 0; m < type.num_meshes; ++m) {
			const auto& mesh = type.model->getMeshes()[m];
			for (int c = 0; c < kMaxChunks; ++c) {
				auto& cmd = commands[m * kMaxChunks + c];
				cmd.count = static_cast<uint32_t>(mesh.indices.size());
				cmd.instanceCount = 0;
				cmd.firstIndex = 0;
				cmd.baseVertex = 0;
				cmd.baseInstance = c * kMaxInstancesPerChunk;
			}
		}
		glBufferData(
			GL_SHADER_STORAGE_BUFFER,
			commands.size() * sizeof(DrawElementsIndirectCommand),
			commands.data(),
			GL_STATIC_DRAW
		);

		decor_types_.push_back(type);
	}

	void DecorManager::Update(
		float /*delta_time*/,
		const Camera&                         camera,
		const Frustum&                        frustum,
		const ITerrainGenerator&              terrain_gen,
		std::shared_ptr<TerrainRenderManager> render_manager
	) {
		if (!enabled_ || !initialized_ || decor_types_.empty())
			return;

		if (!placement_shader_ || !placement_shader_->isValid())
			return;

		if (!render_manager)
			return;

		// Always call _RegeneratePlacements - the shader will handle conditional recomputation
		_RegeneratePlacements(camera, frustum, terrain_gen, render_manager);

		last_camera_pos_ = glm::vec2(camera.x, camera.z);
		last_camera_front_ = camera.front();
		last_terrain_version_ = terrain_gen.GetVersion();
		last_world_scale_ = terrain_gen.GetWorldScale();
		needs_regeneration_ = false;
	}

	void DecorManager::_RegeneratePlacements(
		const Camera&                         camera,
		const Frustum&                        frustum,
		const ITerrainGenerator&              terrain_gen,
		std::shared_ptr<TerrainRenderManager> render_manager
	) {
		float world_scale = terrain_gen.GetWorldScale();

		// Get the heightmap texture array from the terrain render manager
		GLuint heightmap_texture = render_manager->GetHeightmapTexture();
		if (heightmap_texture == 0)
			return;

		// Get all registered chunks
		auto export_info = render_manager->GetChunkExportInfo();
		if (export_info.empty())
			return;

		// Prepare ChunkData SSBO
		std::vector<ChunkData> chunk_data(kMaxChunks, ChunkData{});
		for (const auto& info : export_info) {
			if (info.texture_slice >= 0 && info.texture_slice < kMaxChunks) {
				auto& cd = chunk_data[info.texture_slice];
				cd.world_offset = info.world_offset;
				cd.texture_slice = static_cast<float>(info.texture_slice);
				cd.chunk_size = info.chunk_size;
				cd.min_y = info.min_y;
				cd.max_y = info.max_y;
				cd.version = info.version;
				cd.is_active = 1;
			}
		}

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk_data_ssbo_);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, kMaxChunks * sizeof(ChunkData), chunk_data.data());

		glm::vec2 cam_pos(camera.x, camera.z);

		placement_shader_->use();
		placement_shader_->setVec2("u_cameraPos", cam_pos);
		placement_shader_->setFloat("u_maxTerrainHeight", terrain_gen.GetMaxHeight());
		placement_shader_->setFloat("u_worldScale", world_scale);

		// Scale distance-based parameters by world scale
		placement_shader_->setFloat("u_densityFalloffStart", 200.0f * world_scale);
		placement_shader_->setFloat("u_densityFalloffEnd", 500.0f * world_scale);
		placement_shader_->setFloat("u_maxDecorDistance", 600.0f * world_scale);
		placement_shader_->setInt("u_maxInstancesPerChunk", kMaxInstancesPerChunk);
		placement_shader_->setInt("u_maxChunks", kMaxChunks);

		// Pass frustum planes for GPU-side culling
		for (int p = 0; p < 6; ++p) {
			placement_shader_->setVec4(
				"u_frustumPlanes[" + std::to_string(p) + "]",
				glm::vec4(frustum.planes[p].normal, frustum.planes[p].distance)
			);
		}

		// Bind heightmap texture array
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture);
		placement_shader_->setInt("u_heightmapArray", 0);

		// Bind chunk data and status SSBOs
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, chunk_data_ssbo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, chunk_status_ssbo_);

		// Find the highest texture slice index that is active
		int max_slice = -1;
		for (const auto& info : export_info) {
			if (info.texture_slice > max_slice && info.texture_slice < kMaxChunks) {
				max_slice = info.texture_slice;
			}
		}

		// If no chunks are active, nothing to do
		if (max_slice < 0) return;

		for (size_t i = 0; i < decor_types_.size(); ++i) {
			auto& type = decor_types_[i];

			placement_shader_->setFloat("u_minDensity", type.props.min_density);
			placement_shader_->setFloat("u_maxDensity", type.props.max_density);
			placement_shader_->setFloat("u_baseScale", type.props.base_scale);
			placement_shader_->setFloat("u_scaleVariance", type.props.scale_variance);
			placement_shader_->setFloat("u_minHeight", type.props.min_height);
			placement_shader_->setFloat("u_maxHeight", type.props.max_height);
			placement_shader_->setFloat("u_minSlope", type.props.min_slope);
			placement_shader_->setFloat("u_maxSlope", type.props.max_slope);
			placement_shader_->setVec3("u_baseRotation", glm::radians(type.props.base_rotation));
			placement_shader_->setBool("u_randomYaw", type.props.random_yaw);
			placement_shader_->setBool("u_alignToTerrain", type.props.align_to_terrain);
			placement_shader_->setInt("u_typeIndex", (int)i);
			placement_shader_->setInt("u_numMeshes", type.num_meshes);

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, type.instances_ssbo);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, type.indirect_commands_ssbo);

			// Dispatch compute shader for ALL chunks up to max_slice
			// Each chunk is processed by one workgroup of 16x16 threads, with each thread processing 4 points
			// The Z dimension of the dispatch corresponds to the chunk index
			glDispatchCompute(1, 1, (GLuint)(max_slice + 1));

			// Memory barrier after each type to ensure all writes complete
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}
	}

	void DecorManager::Render(const glm::mat4& view, const glm::mat4& projection) {
		if (!enabled_ || !initialized_ || decor_types_.empty())
			return;

		auto shader = Shape::shader;
		if (!shader)
			return;

		// Ensure SSBO data from compute shader is visible to vertex shader
		// We need COMMAND_BARRIER_BIT for indirect commands
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

		shader->use();
		shader->setMat4("view", view);
		shader->setMat4("projection", projection);
		shader->setMat4("model", glm::mat4(1.0f)); // Identity - instances provide transform
		shader->setBool("useSSBOInstancing", true);
		shader->setBool("isTextEffect", false);
		shader->setBool("isColossal", false);
		shader->setBool("is_instanced", false);
		shader->setVec3("objectColor", 1.0f, 1.0f, 1.0f);
		shader->setBool("usePBR", false);
		shader->setVec4("clipPlane", 0.0f, 0.0f, 0.0f, 0.0f); // Disable clipping for main pass
		shader->setFloat(
			"ripple_strength",
			ConfigManager::GetInstance().GetAppSettingBool("artistic_effect_ripple", false) ? 0.05f : 0.0f
		);

		for (size_t i = 0; i < decor_types_.size(); ++i) {
			auto& type = decor_types_[i];

			// Bind SSBO to binding point 10 (expected by common_instanced.vert)
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, type.instances_ssbo);

			// Bind indirect commands buffer
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, type.indirect_commands_ssbo);

			for (size_t m = 0; m < type.num_meshes; ++m) {
				const auto& mesh = type.model->getMeshes()[m];

				bool hasDiffuse = false;
				for (const auto& t : mesh.textures) {
					if (t.type == "texture_diffuse") {
						hasDiffuse = true;
						break;
					}
				}
				shader->setBool("use_texture", hasDiffuse);
				mesh.bindTextures(*shader);

				glBindVertexArray(mesh.VAO);

				// Each mesh has its own block of kMaxChunks commands in the SSBO
				glMultiDrawElementsIndirect(
					GL_TRIANGLES,
					GL_UNSIGNED_INT,
					reinterpret_cast<void*>(m * kMaxChunks * sizeof(DrawElementsIndirectCommand)),
					kMaxChunks,
					sizeof(DrawElementsIndirectCommand)
				);
			}
		}

		glBindVertexArray(0);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		shader->setBool("useSSBOInstancing", false);
	}

} // namespace Boidsish
