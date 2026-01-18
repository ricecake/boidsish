#pragma once

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
		Text(
			const std::string& text,
			const std::string& font_path,
			float              font_size,
			float              depth,
			int                id = 0,
			float              x = 0.0f,
			float              y = 0.0f,
			float              z = 0.0f,
			float              r = 1.0f,
			float              g = 1.0f,
			float              b = 1.0f,
			float              a = 1.0f
		);
		~Text();

		void      render() const override;
		void      render(Shader& shader, const glm::mat4& model_matrix) const override;
		glm::mat4 GetModelMatrix() const override;

	private:
		void LoadFont(const std::string& font_path);
		void GenerateMesh(const std::string& text, float font_size, float depth);

		std::string text_;
		std::string font_path_;
		float       font_size_;
		float       depth_;

		unsigned int vao_ = 0;
		unsigned int vbo_ = 0;
		int          vertex_count_ = 0;

		// Font data
		std::vector<unsigned char>      font_buffer_;
		std::unique_ptr<stbtt_fontinfo> font_info_;
	};

} // namespace Boidsish
