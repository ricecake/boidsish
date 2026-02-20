#pragma once

#include <memory>
#include <string>
#include <vector>

#include "model.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <shader.h>

namespace Boidsish {

	class SdfGenerator {
	public:
		static SdfGenerator& GetInstance();

		// Generate an SDF for the given model data
		void GenerateSdf(std::shared_ptr<ModelData> data);

		// Initialize shaders and resources
		void Initialize();

	private:
		SdfGenerator() = default;
		~SdfGenerator();

		bool initialized_ = false;

		std::unique_ptr<ComputeShader> voxelize_shader_;
		std::unique_ptr<ComputeShader> jfa_shader_;
		std::unique_ptr<ComputeShader> distance_shader_;

		// Internal helper for JFA
		GLuint _RunJfa(GLuint texture_a, GLuint texture_b, int size);
	};

} // namespace Boidsish
