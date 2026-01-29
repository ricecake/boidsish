#pragma once

#include <memory>
#include <random>
#include <vector>

#include "post_processing/IPostProcessingEffect.h"

class Shader; // Forward declaration

namespace Boidsish {
	namespace PostProcessing {

		class TimeStutterEffect: public IPostProcessingEffect {
		public:
			TimeStutterEffect();
			~TimeStutterEffect();

			void Apply(
				GLuint           sourceTexture,
				GLuint           depthTexture,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) override;
			void Initialize(int width, int height) override;
			void Resize(int width, int height) override;

			void SetTime(float time) override { time_ = time; }

		private:
			void InitializeFBOs(int width, int height);

			std::unique_ptr<Shader> blit_shader_;
			float                   time_ = 0.0f;

			static constexpr int kFrameHistoryCount = 10;
			std::vector<GLuint>  history_fbo_;
			std::vector<GLuint>  history_texture_;
			int                  current_frame_idx_ = 0;

			float last_stutter_time_ = 0.0f;
			float stutter_end_time_ = 0.0f;
			int   displayed_frame_offset_ = 0;

			std::mt19937 rng_;

			int width_ = 0;
			int height_ = 0;
		};

	} // namespace PostProcessing
} // namespace Boidsish
