#include "sdf_generator.h"

#include <algorithm>
#include <vector>

#include "logger.h"
#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {

	SdfGenerator& SdfGenerator::GetInstance() {
		static SdfGenerator instance;
		return instance;
	}

	SdfGenerator::~SdfGenerator() {}

	void SdfGenerator::Initialize() {
		if (initialized_)
			return;

		voxelize_shader_ = std::make_unique<ComputeShader>("shaders/voxelize.comp");
		jfa_shader_ = std::make_unique<ComputeShader>("shaders/sdf_gen.comp");

		if (!voxelize_shader_->isValid() || !jfa_shader_->isValid()) {
			logger::ERROR("SdfGenerator: Failed to compile compute shaders");
		}

		initialized_ = true;
	}

	void SdfGenerator::GenerateSdf(std::shared_ptr<ModelData> data) {
		if (!data || data->sdf_loaded)
			return;

		Initialize();

		const int resolution = 64; // Default resolution for medium scale objects
		data->sdf_extent = data->aabb.max - data->aabb.min;
		data->sdf_min = data->aabb.min;

		// 1. Create 3D textures for ping-pong JFA
		GLuint textures[2];
		glGenTextures(2, textures);
		for (int i = 0; i < 2; ++i) {
			glBindTexture(GL_TEXTURE_3D, textures[i]);
			glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, resolution, resolution, resolution, 0, GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		}

		// 2. Voxelize mesh into textures[0]
		// Prepare geometry buffers
		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;
		unsigned int vertex_offset = 0;
		for (const auto& mesh : data->meshes) {
			for (const auto& v : mesh.vertices) vertices.push_back(v);
			for (unsigned int i : mesh.indices) indices.push_back(i + vertex_offset);
			vertex_offset += mesh.vertices.size();
		}

		GLuint vbo, ebo;
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, vbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

		glGenBuffers(1, &ebo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ebo);
		glBufferData(GL_SHADER_STORAGE_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

		voxelize_shader_->use();
		voxelize_shader_->setVec3("u_localMin", data->aabb.min);
		voxelize_shader_->setVec3("u_localMax", data->aabb.max);
		voxelize_shader_->setInt("u_resolution", resolution);
		voxelize_shader_->setInt("u_numIndices", (int)indices.size());

		glBindImageTexture(0, textures[0], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, vbo);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ebo);

		int groups = (resolution + 7) / 8;
		glDispatchCompute(groups, groups, groups);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		// 3. Run JFA
		GLuint final_jfa_output = _RunJfa(textures[0], textures[1], resolution);

		// 4. Convert seed coordinates to distance
		// We'll reuse the jfa_shader_ logic but with a final pass if needed.
		// Actually, let's create a final distance shader to produce GL_R16F or similar.
		GLuint sdf_tex;
		glGenTextures(1, &sdf_tex);
		glBindTexture(GL_TEXTURE_3D, sdf_tex);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_R16F, resolution, resolution, resolution, 0, GL_RED, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		// Final pass to calculate distance from seeds
		// I'll use a temporary shader for this or just add it to SdfGenerator
		static std::unique_ptr<ComputeShader> final_shader;
		if (!final_shader) {
			final_shader = std::make_unique<ComputeShader>("shaders/sdf_finalize.comp");
		}

		if (final_shader->isValid()) {
			final_shader->use();
			final_shader->setInt("u_resolution", resolution);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_3D, final_jfa_output); // Last output from JFA
			final_shader->setInt("u_seedTexture", 0);
			glBindImageTexture(0, sdf_tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R16F);
			glDispatchCompute(groups, groups, groups);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		}

		data->sdf_texture = sdf_tex;
		data->sdf_loaded = true;

		// Cleanup
		glDeleteTextures(2, textures);
		glDeleteBuffers(1, &vbo);
		glDeleteBuffers(1, &ebo);

		logger::INFO("Generated 3D SDF for model: {} ({}x{}x{})", data->model_path, resolution, resolution, resolution);
	}

	GLuint SdfGenerator::_RunJfa(GLuint tex_a, GLuint tex_b, int size) {
		int    steps = (int)std::ceil(std::log2((float)size));
		GLuint current_input = tex_a;
		GLuint current_output = tex_b;

		jfa_shader_->use();
		jfa_shader_->setInt("u_resolution", size);

		for (int i = 0; i < steps; ++i) {
			int step_size = 1 << (steps - i - 1);
			jfa_shader_->setInt("u_stepSize", step_size);

			glBindImageTexture(0, current_input, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
			glBindImageTexture(1, current_output, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

			int groups = (size + 7) / 8;
			glDispatchCompute(groups, groups, groups);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			std::swap(current_input, current_output);
		}

		return current_input;
	}

} // namespace Boidsish
