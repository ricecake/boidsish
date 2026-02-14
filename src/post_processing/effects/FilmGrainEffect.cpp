#include "post_processing/effects/FilmGrainEffect.h"

#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		FilmGrainEffect::FilmGrainEffect() {
			name_ = "FilmGrain";
			intensity_ = 0.05f;
		}

		FilmGrainEffect::~FilmGrainEffect() {}

		void FilmGrainEffect::Initialize(int /*width*/, int /*height*/) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/film_grain.frag");
		}

		void FilmGrainEffect::Apply(const PostProcessingParams& params) {
			if (!shader_) return;
			shader_->use();
			shader_->setInt("sceneTexture", 0);
			shader_->setFloat("time", params.time);
			shader_->setFloat("intensity", intensity_);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, params.sourceTexture);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void FilmGrainEffect::Resize(int /*width*/, int /*height*/) {}

	} // namespace PostProcessing
} // namespace Boidsish
