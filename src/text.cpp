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
		float              flatness,
		int                id,
		float              x,
		float              y,
		float              z,
		float              r,
		float              g,
		float              b,
		float              a
	):
		Shape(id, x, y, z, r, g, b, a),
		text_(text),
		font_path_(font_path),
		font_size_(font_size),
		depth_(depth),
		justification_(justification),
		flatness_squared_(flatness * flatness) {
		font_info_ = std::make_unique<stbtt_fontinfo>();
		LoadFont(font_path_);
		GenerateMesh(text_, font_size_, depth_);
	}

	namespace {
		void tessellate_quad(
			float                                     x1,
			float                                     y1,
			float                                     x2,
			float                                     y2,
			float                                     x3,
			float                                     y3,
			int                                       level,
			float                                     flatness,
			std::vector<std::array<float, 2>>& tessellated_points
		) {
			if (level > 10) {
				tessellated_points.push_back({x3, y3});
				return;
			}

			float x12 = (x1 + x2) / 2;
			float y12 = (y1 + y2) / 2;
			float x23 = (x2 + x3) / 2;
			float y23 = (y2 + y3) / 2;
			float x123 = (x12 + x23) / 2;
			float y123 = (y12 + y23) / 2;

			float dx = x3 - x1;
			float dy = y3 - y1;
			float d = fabs(((x2 - x3) * dy - (y2 - y3) * dx));

			if (d * d < flatness * (dx * dx + dy * dy)) {
				tessellated_points.push_back({x3, y3});
				return;
			}

			tessellate_quad(x1, y1, x12, y12, x123, y123, level + 1, flatness, tessellated_points);
			tessellate_quad(x123, y123, x23, y23, x3, y3, level + 1, flatness, tessellated_points);
		}
	} // namespace

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
							stbtt_vertex& v = stb_vertices[i];
							float         v_x = v.x * scale;
							float         v_y = v.y * scale + ascent * scale;

							switch (v.type) {
							case STBTT_vmove:
								if (!current_contour.empty()) {
									polygon.push_back(current_contour);
								}
								current_contour.clear();
								current_contour.push_back({v_x, v_y});
								break;
							case STBTT_vline:
								current_contour.push_back({v_x, v_y});
								break;
							case STBTT_vcurve: {
								float v_c_x = v.cx * scale;
								float v_c_y = v.cy * scale + ascent * scale;

								std::vector<std::array<float, 2>> tessellated_points;
								const auto&                       last_point = current_contour.back();
								tessellate_quad(
									last_point[0],
									last_point[1],
									v_c_x,
									v_c_y,
									v_x,
									v_y,
									0,
									flatness_squared_,
									tessellated_points
								);
								current_contour.insert(
									current_contour.end(),
									tessellated_points.begin(),
									tessellated_points.end()
								);
								break;
							}
							default:
								break;
							}
						}
						polygon.push_back(current_contour);

						// Find the outer polygon (largest area) and enforce CCW winding.
						// Enforce CW winding for all other polygons (holes).
						if (!polygon.empty()) {
							std::vector<float> areas(polygon.size());
							for (size_t i = 0; i < polygon.size(); ++i) {
								float area = 0.0f;
								for (size_t j = 0; j < polygon[i].size(); ++j) {
									const auto& p1 = polygon[i][j];
									const auto& p2 = polygon[i][(j + 1) % polygon[i].size()];
									area += (p2[0] - p1[0]) * (p2[1] + p1[1]);
								}
								areas[i] = area;
							}

							size_t outer_polygon_index = 0;
							float  max_area = 0.0f;
							for (size_t i = 0; i < polygon.size(); ++i) {
								if (std::abs(areas[i]) > std::abs(max_area)) {
									max_area = areas[i];
									outer_polygon_index = i;
								}
							}

							for (size_t i = 0; i < polygon.size(); ++i) {
								if (i == outer_polygon_index) {
									// Outer polygon must be CCW (positive area)
									if (areas[i] < 0.0f) {
										std::reverse(polygon[i].begin(), polygon[i].end());
									}
								} else {
									// Hole polygons must be CW (negative area)
									if (areas[i] > 0.0f) {
										std::reverse(polygon[i].begin(), polygon[i].end());
									}
								}
							}

							if (outer_polygon_index != 0) {
								std::swap(polygon[0], polygon[outer_polygon_index]);
							}
						}

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
								{v1[0], v1[1], depth / 2.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f}
							);
							glyph_vertices.insert(
								glyph_vertices.end(),
								{v2[0], v2[1], depth / 2.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f}
							);
							glyph_vertices.insert(
								glyph_vertices.end(),
								{v3[0], v3[1], depth / 2.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f}
							);

							// Back face
							glyph_vertices.insert(
								glyph_vertices.end(),
								{v1[0], v1[1], -depth / 2.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f}
							);
							glyph_vertices.insert(
								glyph_vertices.end(),
								{v3[0], v3[1], -depth / 2.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f}
							);
							glyph_vertices.insert(
								glyph_vertices.end(),
								{v2[0], v2[1], -depth / 2.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f}
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
									{p1_front.x, p1_front.y, p1_front.z, normal.x, normal.y, normal.z, 0.0f, 0.0f}
								);
								glyph_vertices.insert(
									glyph_vertices.end(),
									{p2_front.x, p2_front.y, p2_front.z, normal.x, normal.y, normal.z, 0.0f, 0.0f}
								);
								glyph_vertices.insert(
									glyph_vertices.end(),
									{p1_back.x, p1_back.y, p1_back.z, normal.x, normal.y, normal.z, 0.0f, 0.0f}
								);

								glyph_vertices.insert(
									glyph_vertices.end(),
									{p1_back.x, p1_back.y, p1_back.z, normal.x, normal.y, normal.z, 0.0f, 0.0f}
								);
								glyph_vertices.insert(
									glyph_vertices.end(),
									{p2_front.x, p2_front.y, p2_front.z, normal.x, normal.y, normal.z, 0.0f, 0.0f}
								);
								glyph_vertices.insert(
									glyph_vertices.end(),
									{p2_back.x, p2_back.y, p2_back.z, normal.x, normal.y, normal.z, 0.0f, 0.0f}
								);
							}
						}

						stbtt_FreeShape(font_info_.get(), stb_vertices);
					}
					glyph_cache_[c] = glyph_vertices;
				}

				const std::vector<float>& cached_vertices = glyph_cache_[c];
				for (size_t i = 0; i < cached_vertices.size(); i += 8) {
					vertices.insert(
						vertices.end(),
						{cached_vertices[i] + x_offset,
					     cached_vertices[i + 1] - y_offset,
					     cached_vertices[i + 2],
					     cached_vertices[i + 3],
					     cached_vertices[i + 4],
					     cached_vertices[i + 5],
					     cached_vertices[i + 6],
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

		glBindVertexArray(vao_);
		glDrawArrays(GL_TRIANGLES, 0, vertex_count_);
		glBindVertexArray(0);
	}

	void Text::render(Shader& shader, const glm::mat4& model_matrix) const {
		if (vao_ == 0)
			return;

		shader.use();
		shader.setMat4("model", model_matrix);
		shader.setVec3("objectColor", GetR(), GetG(), GetB());

		glBindVertexArray(vao_);
		glDrawArrays(GL_TRIANGLES, 0, vertex_count_);
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
