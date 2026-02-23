#include "curved_text.h"

#include <array>
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
		const glm::vec3&   wrap_normal,
		const glm::vec3&   text_normal,
		float              duration
	):
		Text(text, font_path, font_size, depth, CENTER, 0, position.x, position.y, position.z, 1, 1, 1, 1, false),
		center_(position),
		radius_(radius),
		angle_rad_(glm::radians(angle_degrees)),
		wrap_normal_(glm::normalize(wrap_normal)),
		text_normal_(glm::normalize(text_normal)),
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

		mesh_vertices_.clear();
		float scale = stbtt_ScaleForPixelHeight(font_info_.get(), font_size);

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

		// Calculate basis vectors for the transformation
		// Wrap Normal (W) is the axis of curvature.
		// Text Normal (N) is the facing direction at the midpoint.
		glm::vec3 W = wrap_normal_;
		glm::vec3 N = text_normal_;
		glm::vec3 X0, Y0, Z0, Radial_mid, RotationAxis;

		if (glm::abs(glm::dot(W, N)) < 0.999f) {
			// Facing is not parallel to Axis -> Can-style wrapping
			// The wrap axis W serves as the text's "Up" direction.
			Y0 = W;
			Z0 = N;
			X0 = glm::normalize(glm::cross(Y0, Z0));
			Radial_mid = Z0;
			RotationAxis = W;
		} else {
			// Facing is parallel to Axis -> Rainbow-style wrapping
			// The wrap axis W is the normal to the plane of the rainbow.
			Z0 = N;
			// Use a reference vector to define the plane of the rainbow.
			glm::vec3 ref = (glm::abs(W.y) < 0.999f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
			glm::vec3 Tangent_mid = glm::normalize(glm::cross(ref, W));
			Radial_mid = glm::normalize(glm::cross(W, Tangent_mid));
			// Text points away from the center of the arc
			Y0 = Radial_mid;
			X0 = glm::normalize(glm::cross(Y0, Z0));
			// We rotate around -W to ensure text flows Left-to-Right when looking at the front.
			RotationAxis = -W;
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
					float vz = cached_vertices[i + 2];

					float normalized_x = (max_width > 0.0f) ? ((cached_vertices[i] + line_accumulated_x) / max_width)
															: 0.0f;

					// Curving transformation
					float     theta = (normalized_x - 0.5f) * angle_rad_;
					glm::mat4 rot_mat = glm::rotate(glm::mat4(1.0f), theta, RotationAxis);
					glm::mat3 rot = glm::mat3(rot_mat);

					glm::vec3 radial = rot * Radial_mid;
					glm::vec3 text_up = rot * Y0;
					glm::vec3 text_face = rot * Z0;

					glm::vec3 final_pos = radius_ * radial + vy * text_up + vz * text_face;

					// Transform normal
					glm::vec3 v_normal = {cached_vertices[i + 3], cached_vertices[i + 4], cached_vertices[i + 5]};
					glm::vec3 final_normal = rot * v_normal;

					Vertex v;
					v.Position = final_pos;
					v.Normal = final_normal;
					v.TexCoords = {normalized_x, cached_vertices[i + 7]};
					v.Color = glm::vec3(1.0f);
					mesh_vertices_.push_back(v);
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

		vertex_count_ = static_cast<int>(mesh_vertices_.size());
	}

} // namespace Boidsish
