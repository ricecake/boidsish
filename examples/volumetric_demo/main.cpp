#include <iostream>
#include <vector>

#include "constants.h"
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
			VolumetricDemoEffect(Visualizer& vis): vis_(vis), width_(0), height_(0) {
				name_ = "VolumetricDemo";
			}
			virtual ~VolumetricDemoEffect() = default;

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
				shader_->setInt("u_noiseTexture", 5);
				shader_->setInt("u_curlTexture", 6);
				shader_->setVec3("cameraPos", cameraPos);
				shader_->setFloat("time", time_);
				shader_->setMat4("invView", glm::inverse(viewMatrix));
				shader_->setMat4("invProjection", glm::inverse(projectionMatrix));

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, sourceTexture);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, depthTexture);

				glActiveTexture(GL_TEXTURE5);
				glBindTexture(GL_TEXTURE_3D, vis_.GetNoiseTexture());
				glActiveTexture(GL_TEXTURE6);
				glBindTexture(GL_TEXTURE_3D, vis_.GetCurlTexture());

				glDrawArrays(GL_TRIANGLES, 0, 6);

				glActiveTexture(GL_TEXTURE6);
				glBindTexture(GL_TEXTURE_3D, 0);
				glActiveTexture(GL_TEXTURE5);
				glBindTexture(GL_TEXTURE_3D, 0);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, 0);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, 0);
			}

			void Resize(int width, int height) override {
				width_ = width;
				height_ = height;
			}

			void SetTime(float time) override { time_ = time; }

			bool IsEarly() const override { return true; }

		private:
			Visualizer&             vis_;
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
			auto effect = std::make_shared<Boidsish::PostProcessing::VolumetricDemoEffect>(v);
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
