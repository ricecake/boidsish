#include "decor_manager.h"

#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

#include "ConfigManager.h"
#include "graphics.h"
#include "logger.h"
#include "terrain_generator.h"

namespace Boidsish {

	DecorManager::DecorManager() {}

	DecorManager::~DecorManager() {
		for (auto& type : decor_types_) {
			if (type.ssbo != 0)
				glDeleteBuffers(1, &type.ssbo);
			if (type.count_buffer != 0)
				glDeleteBuffers(1, &type.count_buffer);
		}
		if (heightmap_texture_ != 0)
			glDeleteTextures(1, &heightmap_texture_);
	}

	void DecorManager::_Initialize() {
		if (initialized_)
			return;

		placement_shader_ = std::make_unique<ComputeShader>("shaders/decor_placement.comp");
		// Use the same shader as for regular objects but we might need some tweaks for instancing
		// Actually, InstanceManager uses 'shader' which is vis.vert/frag.
		// Model::render() uses Shape::shader.

		glGenTextures(1, &heightmap_texture_);
		glBindTexture(GL_TEXTURE_2D, heightmap_texture_);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		initialized_ = true;
	}

	void DecorManager::AddDecorType(const std::string& model_path, float density) {
		_Initialize();

		DecorType type;
		type.model = std::make_shared<Model>(model_path);
		type.density = density;

		glGenBuffers(1, &type.ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, type.ssbo);
		// Each instance is a mat4 (64 bytes)
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxInstancesPerType * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);

		glGenBuffers(1, &type.count_buffer);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, type.count_buffer);
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(unsigned int), nullptr, GL_DYNAMIC_DRAW);

		decor_types_.push_back(type);
	}

	void DecorManager::Update(float /*delta_time*/, const Camera& camera, const TerrainGenerator& terrain_gen) {
		if (!enabled_ || !initialized_ || decor_types_.empty())
			return;

		// Update heightmap if camera moved too far
		glm::vec2 cam_pos(camera.x, camera.z);
		float     dist = glm::distance(cam_pos, heightmap_world_pos_ + glm::vec2(heightmap_size_ * 0.5f));
		if (dist > heightmap_size_ * 0.25f || heightmap_world_pos_ == glm::vec2(0.0f)) {
			heightmap_world_pos_ = cam_pos - glm::vec2(heightmap_size_ * 0.5f);
			auto heightmap_data = terrain_gen.GenerateTextureForArea(
				(int)heightmap_world_pos_.x,
				(int)heightmap_world_pos_.y,
				(int)heightmap_size_
			);

			glBindTexture(GL_TEXTURE_2D, heightmap_texture_);
			// GenerateTextureForArea returns RGBA16 (uint16_t * 4 per pixel)
			// Channel 3 (Alpha) is height. Normal is in RGB.
			glTexImage2D(
				GL_TEXTURE_2D,
				0,
				GL_RGBA16,
				(int)heightmap_size_,
				(int)heightmap_size_,
				0,
				GL_RGBA,
				GL_UNSIGNED_SHORT,
				heightmap_data.data()
			);
		}

		placement_shader_->use();
		placement_shader_->setVec2("u_cameraPos", cam_pos);
		placement_shader_->setVec2("u_heightmapWorldPos", heightmap_world_pos_);
		placement_shader_->setFloat("u_heightmapWorldSize", heightmap_size_);
		placement_shader_->setFloat("u_maxTerrainHeight", terrain_gen.GetMaxHeight());

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, heightmap_texture_);
		placement_shader_->setInt("u_heightmap", 0);

		for (size_t i = 0; i < decor_types_.size(); ++i) {
			auto& type = decor_types_[i];

			// Reset atomic counter
			unsigned int zero = 0;
			glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, type.count_buffer);
			glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(unsigned int), &zero);

			placement_shader_->setFloat("u_density", type.density);
			placement_shader_->setFloat("u_baseScale", type.base_scale);
			placement_shader_->setFloat("u_scaleVariance", type.scale_variance);
			placement_shader_->setFloat("u_minHeight", type.min_height);
			placement_shader_->setFloat("u_maxHeight", type.max_height);
			placement_shader_->setFloat("u_minSlope", type.min_slope);
			placement_shader_->setFloat("u_maxSlope", type.max_slope);
			placement_shader_->setInt("u_typeIndex", (int)i);

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, type.ssbo);
			glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, type.count_buffer);

			// Dispatch based on grid. e.g. 64x64 grid points per decor type around camera
			glDispatchCompute(64 / 8, 64 / 8, 1);
		}

		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);
	}

	void DecorManager::Render(const glm::mat4& view, const glm::mat4& projection) {
		if (!enabled_ || !initialized_ || decor_types_.empty())
			return;

		auto shader = Shape::shader;
		if (!shader)
			return;

		shader->use();
		shader->setMat4("view", view);
		shader->setMat4("projection", projection);
		shader->setBool("useSSBOInstancing", true);
		shader->setBool("isColossal", false);
		shader->setBool("is_instanced", false);
		shader->setVec3("objectColor", 1.0f, 1.0f, 1.0f);
		shader->setBool("usePBR", false);
		shader->setVec4("clipPlane", 0.0f, 0.0f, 0.0f, 0.0f); // Disable clipping for main pass
		shader->setFloat(
			"ripple_strength",
			ConfigManager::GetInstance().GetAppSettingBool("artistic_effect_ripple", false) ? 0.05f : 0.0f
		);

		for (auto& type : decor_types_) {
			// Read back count
			unsigned int count = 0;
			glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, type.count_buffer);
			glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(unsigned int), &count);

			if (count == 0)
				continue;

			if (count > kMaxInstancesPerType)
				count = kMaxInstancesPerType;

			// Bind SSBO to a known binding point that the shader expects for instance matrices
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, type.ssbo);

			for (const auto& mesh : type.model->getMeshes()) {
				mesh.bindTextures(*shader);
				mesh.render_instanced((int)count);
			}
		}

		shader->setBool("useSSBOInstancing", false);
	}

} // namespace Boidsish
