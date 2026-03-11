#include <iostream>
#include <vector>

#include "fire_effect.h"
#include "graphics.h"
#include "logger.h"
#include "post_processing/IPostProcessingEffect.h"
#include "post_processing/PostProcessingManager.h"
#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		class VolumetricDemoEffect: public IPostProcessingEffect {
		public:
			VolumetricDemoEffect(): width_(0), height_(0) {}
			virtual ~VolumetricDemoEffect() = default;

			std::string GetName() const override { return "VolumetricDemo"; }

			void Initialize(int width, int height) override {
				width_ = width;
				height_ = height;
				shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/volumetric_demo.frag");
			}

			void Apply(
				GLuint           sourceTexture,
				GLuint           depthTexture,
				GLuint           /*velocityTexture*/,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) override {
				shader_->use();
				shader_->setInt("sceneTexture", 0);
				shader_->setInt("depthTexture", 1);
				shader_->setVec3("cameraPos", cameraPos);
				shader_->setFloat("time", time_);
				shader_->setMat4("invView", glm::inverse(viewMatrix));
				shader_->setMat4("invProjection", glm::inverse(projectionMatrix));

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, sourceTexture);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, depthTexture);

				// Render fullscreen quad
				// The VAO is managed by PostProcessingManager
			}

			void Resize(int width, int height) override {
				width_ = width;
				height_ = height;
			}

			void SetTime(float time) override { time_ = time; }

			bool IsEarly() const override { return true; }

		private:
			std::unique_ptr<Shader> shader_;
			int                     width_;
			int                     height_;
			float                   time_ = 0.0f;
		};

	} // namespace PostProcessing
} // namespace Boidsish

int main() {
	try {
		Boidsish::Visualizer vis(1280, 720, "Volumetric Particle Demo");

		vis.GetCamera().y = 10.0;
		vis.GetCamera().z = 50.0;
		vis.SetCameraMode(Boidsish::CameraMode::FREE);

		// Add a bunch of Volumetric particles
		for (int i = 0; i < 5; ++i) {
			vis.AddFireEffect(
				glm::vec3(-20.0f + i * 10.0f, 5.0f, 0.0f),
				Boidsish::FireEffectStyle::Volumetric,
				{0.0f, 1.0f, 0.0f},
				glm::vec3(0),
				2000 // max particles per emitter
			);
		}

		// Register the custom post-processing effect
		vis.AddPrepareCallback([](Boidsish::Visualizer& v) {
			auto effect = std::make_shared<Boidsish::PostProcessing::VolumetricDemoEffect>();
			v.GetPostProcessingManager().AddEffect(effect);
			effect->SetEnabled(true);
		});

		vis.Run();
	} catch (const std::exception& e) {
		std::cerr << "An error occurred: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
