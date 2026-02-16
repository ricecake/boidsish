#include "text.h"

#include <array>
#include <fstream>
#include <iostream>
#include <vector>

#include "earcut.hpp"
#include "shader.h"
#include "stb_truetype.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {

	Text::Text(
		const std::string& text,
		const std::string& font_path,
		float              font_size,
		float              depth,
		Justification      justification,
		int                id,
		float              x,
		float              y,
		float              z,
		float              r,
		float              g,
		float              b,
		float              a,
		bool               generate_mesh
	):
		Shape(id, x, y, z, r, g, b, a),
		text_(text),
		font_path_(font_path),
		font_size_(font_size),
		depth_(depth),
		justification_(justification) {
		font_info_ = std::make_unique<stbtt_fontinfo>();
		LoadFont(font_path_);
		if (generate_mesh) {
			GenerateMesh(text_, font_size_, depth_);
		}
	}

	Text::~Text() {
		if (vao_ != 0) {
			glDeleteVertexArrays(1, &vao_);
			glDeleteBuffers(1, &vbo_);
		}
	}

	void Text::LoadFont(const std::string& font_path) {
		std::ifstream file(font_path, std::ios::binary | std::ios::ate);
		if (!file.is_open()) {
			std::cerr << "FATAL: Failed to open font file: " << font_path << std::endl;
			exit(1);
		}

		std::streamsize size = file.tellg();
		file.seekg(0, std::ios::beg);

		font_buffer_.resize(size);
		if (!file.read(reinterpret_cast<char*>(font_buffer_.data()), size)) {
			std::cerr << "Failed to read font file: " << font_path << std::endl;
			return;
		}

		if (!stbtt_InitFont(font_info_.get(), font_buffer_.data(), 0)) {
			std::cerr << "Failed to initialize font: " << font_path << std::endl;
		}
	}

	void Text::SetText(const std::string& text) {
		text_ = text;
		GenerateMesh(text_, font_size_, depth_);
	}

	void Text::SetJustification(Justification justification) {
		justification_ = justification;
		GenerateMesh(text_, font_size_, depth_);
	}

	void Text::GenerateMesh(const std::string& text, float font_size, float depth) {
		if (font_buffer_.empty()) {
			return;
		}

		if (vao_ != 0) {
			glDeleteVertexArrays(1, &vao_);
			glDeleteBuffers(1, &vbo_);
		}

		std::vector<float> vertices;
		float              scale = stbtt_ScaleForPixelHeight(font_info_.get(), font_size);

		int ascent, descent, line_gap;
		stbtt_GetFontVMetrics(font_info_.get(), &ascent, &descent, &line_gap);
		float line_height = (ascent - descent + line_gap) * scale;

		std::vector<std::string> lines;
		std::string              current_line;
		for (char c : text) {
			if (c == '\n') {
				lines.push_back(current_line);
				current_line.clear();
			} else {
				current_line += c;
			}
		}
		lines.push_back(current_line);

		float y_offset = 0.0f;

		float max_width = 0.0f;
		for (const auto& line : lines) {
			float line_width = 0.0f;
			for (char c : line) {
				int advance_width, left_side_bearing;
				stbtt_GetGlyphHMetrics(
					font_info_.get(),
					stbtt_FindGlyphIndex(font_info_.get(), c),
					&advance_width,
					&left_side_bearing
				);
				line_width += advance_width * scale;
			}
			if (line_width > max_width)
				max_width = line_width;
		}

		for (const auto& line : lines) {
			float line_width = 0.0f;
			for (char c : line) {
				int advance_width, left_side_bearing;
				stbtt_GetGlyphHMetrics(
					font_info_.get(),
					stbtt_FindGlyphIndex(font_info_.get(), c),
					&advance_width,
					&left_side_bearing
				);
				line_width += advance_width * scale;
			}

			float x_offset = 0.0f;
			switch (justification_) {
			case CENTER:
				x_offset = -line_width / 2.0f;
				break;
			case RIGHT:
				x_offset = -line_width;
				break;
			case LEFT:
			default:
				x_offset = 0.0f;
				break;
			}

			float line_accumulated_x = 0.0f;
			for (char c : line) {
				if (glyph_cache_.find(c) == glyph_cache_.end()) {
					int glyph_index = stbtt_FindGlyphIndex(font_info_.get(), c);

					stbtt_vertex* stb_vertices;
					int           num_vertices = stbtt_GetGlyphShape(font_info_.get(), glyph_index, &stb_vertices);

					std::vector<float> glyph_vertices;

					if (num_vertices > 0) {
						std::vector<std::vector<std::array<float, 2>>> polygon;
						std::vector<std::array<float, 2>>              current_contour;

						for (int i = 0; i < num_vertices; ++i) {
							stbtt_vertex v = stb_vertices[i];
							// stb_truetype glyph shape uses Y-up coordinates in font units.
							current_contour.push_back({(v.x * scale), (v.y * scale)});

							if (i < num_vertices - 1 && stb_vertices[i + 1].type == STBTT_vmove) {
								polygon.push_back(current_contour);
								current_contour.clear();
							}
						}
						polygon.push_back(current_contour);

						std::vector<std::array<float, 2>> flat_vertices;
						for (const auto& contour : polygon) {
							flat_vertices.insert(flat_vertices.end(), contour.begin(), contour.end());
						}

						std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(polygon);

						for (size_t i = 0; i < indices.size(); i += 3) {
							const auto& v1 = flat_vertices[indices[i]];
							const auto& v2 = flat_vertices[indices[i + 1]];
							const auto& v3 = flat_vertices[indices[i + 2]];

							// Front face (v1, v3, v2) - TrueType contours are CW, so v1,v3,v2 is CCW
							glyph_vertices.insert(
								glyph_vertices.end(),
								{v1[0], v1[1], depth / 2.0f, 0.0f, 0.0f, 1.0f, v1[0], v1[1]}
							);
							glyph_vertices.insert(
								glyph_vertices.end(),
								{v3[0], v3[1], depth / 2.0f, 0.0f, 0.0f, 1.0f, v3[0], v3[1]}
							);
							glyph_vertices.insert(
								glyph_vertices.end(),
								{v2[0], v2[1], depth / 2.0f, 0.0f, 0.0f, 1.0f, v2[0], v2[1]}
							);

							// Back face (v1, v2, v3) - CW from front, CCW from back
							glyph_vertices.insert(
								glyph_vertices.end(),
								{v1[0], v1[1], -depth / 2.0f, 0.0f, 0.0f, -1.0f, v1[0], v1[1]}
							);
							glyph_vertices.insert(
								glyph_vertices.end(),
								{v2[0], v2[1], -depth / 2.0f, 0.0f, 0.0f, -1.0f, v2[0], v2[1]}
							);
							glyph_vertices.insert(
								glyph_vertices.end(),
								{v3[0], v3[1], -depth / 2.0f, 0.0f, 0.0f, -1.0f, v3[0], v3[1]}
							);
						}

						for (const auto& contour : polygon) {
							for (size_t i = 0; i < contour.size(); ++i) {
								size_t    next_i = (i + 1) % contour.size();
								glm::vec3 p1_front = {contour[i][0], contour[i][1], depth / 2.0f};
								glm::vec3 p2_front = {contour[next_i][0], contour[next_i][1], depth / 2.0f};
								glm::vec3 p1_back = {contour[i][0], contour[i][1], -depth / 2.0f};
								glm::vec3 p2_back = {contour[next_i][0], contour[next_i][1], -depth / 2.0f};

								// Outer contour is CW: p1 -> p2
								// For side faces, we want normal pointing out.
								// cross(p2_front - p1_front, p1_back - p1_front)
								glm::vec3 normal = glm::normalize(glm::cross(p2_front - p1_front, p1_back - p1_front));

								// Triangle 1: p1_front, p1_back, p2_back (CCW)
								glyph_vertices.insert(
									glyph_vertices.end(),
									{p1_front.x,
								     p1_front.y,
								     p1_front.z,
								     normal.x,
								     normal.y,
								     normal.z,
								     p1_front.x,
								     p1_front.y}
								);
								glyph_vertices.insert(
									glyph_vertices.end(),
									{p1_back.x,
								     p1_back.y,
								     p1_back.z,
								     normal.x,
								     normal.y,
								     normal.z,
								     p1_back.x,
								     p1_back.y}
								);
								glyph_vertices.insert(
									glyph_vertices.end(),
									{p2_back.x,
								     p2_back.y,
								     p2_back.z,
								     normal.x,
								     normal.y,
								     normal.z,
								     p2_back.x,
								     p2_back.y}
								);

								// Triangle 2: p1_front, p2_back, p2_front (CCW)
								glyph_vertices.insert(
									glyph_vertices.end(),
									{p1_front.x,
								     p1_front.y,
								     p1_front.z,
								     normal.x,
								     normal.y,
								     normal.z,
								     p1_front.x,
								     p1_front.y}
								);
								glyph_vertices.insert(
									glyph_vertices.end(),
									{p2_back.x,
								     p2_back.y,
								     p2_back.z,
								     normal.x,
								     normal.y,
								     normal.z,
								     p2_back.x,
								     p2_back.y}
								);
								glyph_vertices.insert(
									glyph_vertices.end(),
									{p2_front.x,
								     p2_front.y,
								     p2_front.z,
								     normal.x,
								     normal.y,
								     normal.z,
								     p2_front.x,
								     p2_front.y}
								);
							}
						}

						stbtt_FreeShape(font_info_.get(), stb_vertices);
					}
					glyph_cache_[c] = glyph_vertices;
				}

				const std::vector<float>& cached_vertices = glyph_cache_[c];
				for (size_t i = 0; i < cached_vertices.size(); i += 8) {
					float vx = cached_vertices[i] + x_offset;
					float vy = cached_vertices[i + 1] - y_offset;
					float normalized_x = (max_width > 0.0f) ? ((cached_vertices[i] + line_accumulated_x) / max_width)
															: 0.0f;

					vertices.insert(
						vertices.end(),
						{vx,
					     vy,
					     cached_vertices[i + 2],
					     cached_vertices[i + 3],
					     cached_vertices[i + 4],
					     cached_vertices[i + 5],
					     normalized_x,
					     cached_vertices[i + 7]}
					);
				}

				int advance_width, left_side_bearing;
				stbtt_GetGlyphHMetrics(
					font_info_.get(),
					stbtt_FindGlyphIndex(font_info_.get(), c),
					&advance_width,
					&left_side_bearing
				);
				x_offset += advance_width * scale;
				line_accumulated_x += advance_width * scale;
			}
			y_offset += line_height;
		}

		vertex_count_ = vertices.size() / 8;

		glGenVertexArrays(1, &vao_);
		glBindVertexArray(vao_);

		glGenBuffers(1, &vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);
	}

	void Text::render() const {
		if (vao_ == 0 || shader == nullptr)
			return;

		shader->use();
		shader->setMat4("model", GetModelMatrix());
		shader->setVec3("objectColor", GetR(), GetG(), GetB());
		shader->setFloat("objectAlpha", GetA());
		shader->setBool("use_texture", false);

		shader->setBool("isTextEffect", is_text_effect_);
		if (is_text_effect_) {
			shader->setFloat("textFadeProgress", text_fade_progress_);
			shader->setFloat("textFadeSoftness", text_fade_softness_);
			shader->setInt("textFadeMode", text_fade_mode_);
		}

		// Set PBR material properties
		shader->setBool("usePBR", UsePBR());
		if (UsePBR()) {
			shader->setFloat("roughness", GetRoughness());
			shader->setFloat("metallic", GetMetallic());
			shader->setFloat("ao", GetAO());
		}

		glBindVertexArray(vao_);
		glDrawArrays(GL_TRIANGLES, 0, vertex_count_);
		shader->setBool("isTextEffect", false);
		glBindVertexArray(0);
	}

	void Text::render(Shader& shader, const glm::mat4& model_matrix) const {
		if (vao_ == 0)
			return;

		shader.use();
		shader.setMat4("model", model_matrix);
		shader.setVec3("objectColor", GetR(), GetG(), GetB());
		shader.setFloat("objectAlpha", GetA());
		shader.setBool("use_texture", false);

		shader.setBool("isTextEffect", is_text_effect_);
		if (is_text_effect_) {
			shader.setFloat("textFadeProgress", text_fade_progress_);
			shader.setFloat("textFadeSoftness", text_fade_softness_);
			shader.setInt("textFadeMode", text_fade_mode_);
		}

		// Set PBR material properties
		shader.setBool("usePBR", UsePBR());
		if (UsePBR()) {
			shader.setFloat("roughness", GetRoughness());
			shader.setFloat("metallic", GetMetallic());
			shader.setFloat("ao", GetAO());
		}

		glBindVertexArray(vao_);
		glDrawArrays(GL_TRIANGLES, 0, vertex_count_);
		shader.setBool("isTextEffect", false);
		glBindVertexArray(0);
	}

	glm::mat4 Text::GetModelMatrix() const {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
		model = model * glm::mat4_cast(GetRotation());
		model = glm::scale(model, GetScale());
		return model;
	}

} // namespace Boidsish
