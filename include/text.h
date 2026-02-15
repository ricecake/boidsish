#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "shape.h"
#include <glm/glm.hpp>

// Forward declare stb_truetype types to avoid including the full header here
struct stbtt_fontinfo;

namespace Boidsish {
	class Text: public Shape {
	public:
		enum Justification { LEFT, CENTER, RIGHT };

		Text(
			const std::string& text,
			const std::string& font_path,
			float              font_size,
			float              depth,
			Justification      justification = LEFT,
			int                id = 0,
			float              x = 0.0f,
			float              y = 0.0f,
			float              z = 0.0f,
			float              r = 1.0f,
			float              g = 1.0f,
			float              b = 1.0f,
			float              a = 1.0f,
			bool               generate_mesh = true
		);
		~Text();

		virtual void render() const override;
		virtual void render(Shader& shader, const glm::mat4& model_matrix, const glm::mat4& prev_model_matrix) const override;
		glm::mat4    GetModelMatrix() const override;

		// Text objects are not instanced (each has unique geometry)
		std::string GetInstanceKey() const override { return "Text:" + std::to_string(GetId()); }

		void SetText(const std::string& text);
		void SetJustification(Justification justification);

		bool IsTransparent() const override { return GetA() < 0.99f || is_text_effect_; }

		// Text effect state
		void SetTextEffect(bool enabled) { is_text_effect_ = enabled; }

		void SetFadeProgress(float progress) { text_fade_progress_ = progress; }

		void SetFadeSoftness(float softness) { text_fade_softness_ = softness; }

		void SetFadeMode(int mode) { text_fade_mode_ = mode; }

	protected:
		void         LoadFont(const std::string& font_path);
		virtual void GenerateMesh(const std::string& text, float font_size, float depth);

		std::string                        text_;
		std::map<char, std::vector<float>> glyph_cache_;
		std::string                        font_path_;
		float                              font_size_;
		float                              depth_;
		Justification                      justification_;

		unsigned int vao_ = 0;
		unsigned int vbo_ = 0;
		int          vertex_count_ = 0;

		bool  is_text_effect_ = false;
		float text_fade_progress_ = 1.0f;
		float text_fade_softness_ = 0.1f;
		int   text_fade_mode_ = 0;

		// Font data
		std::vector<unsigned char>      font_buffer_;
		std::unique_ptr<stbtt_fontinfo> font_info_;
	};

} // namespace Boidsish
