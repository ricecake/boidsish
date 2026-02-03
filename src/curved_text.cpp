#include "curved_text.h"

#include <iostream>
#include <vector>

#include "earcut.hpp"
#include "shader.h"
#include "stb_truetype.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {

	CurvedText::CurvedText(
		const std::string& text,
		const std::string& font_path,
		float              font_size,
		float              depth,
		const glm::vec3&   position,
		float              radius,
		float              angle_degrees,
		const glm::vec3&   normal,
		float              duration
	):
		Text(text, font_path, font_size, depth, CENTER, 0, position.x, position.y, position.z, 1, 1, 1, 1, false),
		center_(position),
		radius_(radius),
		angle_rad_(glm::radians(angle_degrees)),
		normal_(glm::normalize(normal)),
		total_duration_(duration) {
		is_text_effect_ = true;
		text_fade_progress_ = 0.0f;
		text_fade_mode_ = 0; // Fade In
		GenerateMesh(text_, font_size_, depth_);
	}

	void CurvedText::Update(float delta_time) {
		age_ += delta_time;

		if (age_ < fade_in_time_) {
			text_fade_progress_ = age_ / fade_in_time_;
			text_fade_mode_ = 0;
		} else if (age_ < total_duration_ - fade_out_time_) {
			text_fade_progress_ = 1.0f;
			text_fade_mode_ = 0;
		} else if (age_ < total_duration_) {
			text_fade_progress_ = (age_ - (total_duration_ - fade_out_time_)) / fade_out_time_;
			text_fade_mode_ = 1;
		} else {
			text_fade_progress_ = 1.0f;
			text_fade_mode_ = 1;
			SetHidden(true);
		}
	}

	void CurvedText::render() const {
		Text::render();
	}

	void CurvedText::GenerateMesh(const std::string& text, float font_size, float depth) {
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

		// Calculate basis vectors for the normal
		glm::vec3 up = normal_;
		glm::vec3 right;
		if (std::abs(up.y) < 0.999f) {
			right = glm::normalize(glm::cross(glm::vec3(0, 1, 0), up));
		} else {
			right = glm::normalize(glm::cross(glm::vec3(1, 0, 0), up));
		}
		glm::vec3 forward = glm::normalize(glm::cross(up, right));

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
							current_contour.push_back({(v.x * scale), (v.y * scale) + (ascent * scale)});

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

							// Front face
							glyph_vertices.insert(
								glyph_vertices.end(),
								{v1[0], v1[1], depth / 2.0f, 0.0f, 0.0f, 1.0f, v1[0], v1[1]}
							);
							glyph_vertices.insert(
								glyph_vertices.end(),
								{v2[0], v2[1], depth / 2.0f, 0.0f, 0.0f, 1.0f, v2[0], v2[1]}
							);
							glyph_vertices.insert(
								glyph_vertices.end(),
								{v3[0], v3[1], depth / 2.0f, 0.0f, 0.0f, 1.0f, v3[0], v3[1]}
							);

							// Back face
							glyph_vertices.insert(
								glyph_vertices.end(),
								{v1[0], v1[1], -depth / 2.0f, 0.0f, 0.0f, -1.0f, v1[0], v1[1]}
							);
							glyph_vertices.insert(
								glyph_vertices.end(),
								{v3[0], v3[1], -depth / 2.0f, 0.0f, 0.0f, -1.0f, v3[0], v3[1]}
							);
							glyph_vertices.insert(
								glyph_vertices.end(),
								{v2[0], v2[1], -depth / 2.0f, 0.0f, 0.0f, -1.0f, v2[0], v2[1]}
							);
						}

						for (const auto& contour : polygon) {
							for (size_t i = 0; i < contour.size(); ++i) {
								size_t    next_i = (i + 1) % contour.size();
								glm::vec3 p1_front = {contour[i][0], contour[i][1], depth / 2.0f};
								glm::vec3 p2_front = {contour[next_i][0], contour[next_i][1], depth / 2.0f};
								glm::vec3 p1_back = {contour[i][0], contour[i][1], -depth / 2.0f};
								glm::vec3 p2_back = {contour[next_i][0], contour[next_i][1], -depth / 2.0f};

								glm::vec3 normal = glm::normalize(glm::cross(p2_front - p1_front, p1_back - p1_front));

								glyph_vertices.insert(
									glyph_vertices.end(),
									{p1_front.x, p1_front.y, p1_front.z, normal.x, normal.y, normal.z, p1_front.x, p1_front.y}
								);
								glyph_vertices.insert(
									glyph_vertices.end(),
									{p2_front.x, p2_front.y, p2_front.z, normal.x, normal.y, normal.z, p2_front.x, p2_front.y}
								);
								glyph_vertices.insert(
									glyph_vertices.end(),
									{p1_back.x, p1_back.y, p1_back.z, normal.x, normal.y, normal.z, p1_back.x, p1_back.y}
								);

								glyph_vertices.insert(
									glyph_vertices.end(),
									{p1_back.x, p1_back.y, p1_back.z, normal.x, normal.y, normal.z, p1_back.x, p1_back.y}
								);
								glyph_vertices.insert(
									glyph_vertices.end(),
									{p2_front.x, p2_front.y, p2_front.z, normal.x, normal.y, normal.z, p2_front.x, p2_front.y}
								);
								glyph_vertices.insert(
									glyph_vertices.end(),
									{p2_back.x, p2_back.y, p2_back.z, normal.x, normal.y, normal.z, p2_back.x, p2_back.y}
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
					float vz = cached_vertices[i + 2];

					float normalized_x =
						(max_width > 0.0f) ? ((cached_vertices[i] + line_accumulated_x) / max_width) : 0.0f;

					// Curving transformation
					float     theta = (normalized_x - 0.5f) * angle_rad_;
					glm::vec3 radial = std::cos(theta) * right + std::sin(theta) * forward;
					glm::vec3 final_pos = (radius_ + vz) * radial + vy * normal_;

					// Transform normal as well
					glm::vec3 v_normal = {cached_vertices[i + 3], cached_vertices[i + 4], cached_vertices[i + 5]};
					// Rough approximation of normal rotation: rotate it by theta around 'up'
					glm::mat3 rot = glm::mat3(glm::rotate(glm::mat4(1.0f), theta, up));
					glm::vec3 final_normal = rot * v_normal;

					vertices.insert(
						vertices.end(),
						{final_pos.x,
					     final_pos.y,
					     final_pos.z,
					     final_normal.x,
					     final_normal.y,
					     final_normal.z,
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

} // namespace Boidsish
