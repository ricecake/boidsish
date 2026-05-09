#pragma once

#include <memory>

#include "post_processing/IPostProcessingEffect.h"

class Shader;

namespace Boidsish {
	namespace PostProcessing {

		class PulseEffect : public IPostProcessingEffect {
		public:
			PulseEffect();
			~PulseEffect();

			void Initialize(int width, int height) override;
			void Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) override;
			void Resize(int width, int height) override;

			void SetTime(float time) override { time_ = time; }

			void Trigger(const glm::vec3& origin);

			void  SetBrightness(float brightness) { brightness_ = brightness; }
			float GetBrightness() const { return brightness_; }
			void  SetAmbientBrightness(float ambient) { ambient_brightness_ = ambient; }
			float GetAmbientBrightness() const { return ambient_brightness_; }
			void  SetMaxBounces(int bounces) { max_bounces_ = bounces; }
			int   GetMaxBounces() const { return max_bounces_; }
			void  SetSpeed(float speed) { speed_ = speed; }
			float GetSpeed() const { return speed_; }
			void  SetPulseDuration(float duration) { pulse_duration_ = duration; }
			float GetPulseDuration() const { return pulse_duration_; }

		private:
			void InitializeFBO(int width, int height);
			void InitializeVAO();

			std::unique_ptr<Shader> ray_shader_;
			std::unique_ptr<Shader> composite_shader_;

			GLuint pulse_fbo_ = 0;
			GLuint pulse_texture_ = 0;
			GLuint ray_vao_ = 0;
			GLuint quad_vao_ = 0;
			GLuint quad_vbo_ = 0;

			int width_ = 0;
			int height_ = 0;

			float time_ = 0.0f;
			float last_trigger_time_ = -10.0f;
			glm::vec3 pulse_origin_ = glm::vec3(0.0f);
			bool is_pulsing_ = false;

			float brightness_ = 1.0f;
			float ambient_brightness_ = 0.1f;
			int   max_bounces_ = 3;
			float speed_ = 20.0f;
			float pulse_duration_ = 5.0f;
		};

	} // namespace PostProcessing
} // namespace Boidsish
