#pragma once

#include <memory>
#include <vector>

#include "constants.h"
#include "shader.h"
#include "shape.h"
#include <glm/glm.hpp>

namespace Boidsish {

	struct MeshExplosionFragment {
		glm::vec4 v0;     // local pos0.xyz, unused (w)
		glm::vec4 v1;     // local pos1.xyz, unused (w)
		glm::vec4 v2;     // local pos2.xyz, unused (w)
		glm::vec4 t01;    // tex0.xy, tex1.xy
		glm::vec4 t2_age; // tex2.xy, age (z), lifetime (w)
		glm::vec4 normal; // normal.xyz, unused (w)
		glm::vec4 pos;    // world position (xyz), unused (w)
		glm::vec4 vel;    // world velocity (xyz), unused (w)
		glm::vec4 rot;    // rotation quaternion
		glm::vec4 angVel; // angular velocity axis (xyz) * speed (w)
		glm::vec4 color;  // color (rgb), alpha (a)
	};

	class MeshExplosionManager {
	public:
		MeshExplosionManager();
		~MeshExplosionManager();

		// Initialize shaders and buffers. Must be called from main thread with OpenGL context.
		void Initialize();

		void
		ExplodeShape(std::shared_ptr<Shape> shape, float intensity = 1.0f, const glm::vec3& velocity = glm::vec3(0.0f));
		void Update(float delta_time, float time);
		void Render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camera_pos);

	private:
		void _Initialize();

		bool                           initialized_ = false;
		unsigned int                   ssbo_ = 0;
		unsigned int                   vao_ = 0;
		std::unique_ptr<Shader>        render_shader_;
		std::unique_ptr<ComputeShader> compute_shader_;
		float                          time_ = 0.0f;

		static constexpr int kMaxFragments = Constants::Class::Explosions::MaxFragments();
		int                  current_fragment_index_ = 0;
	};

} // namespace Boidsish
