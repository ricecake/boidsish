#include "text.h"
#include "shader.h"
#include <iostream>
#include <vector>
#include <fstream>

#include "stb_truetype.h"
#include "earcut.hpp"
#include <array>
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {

Text::Text(
    const std::string& text,
    const std::string& font_path,
    float font_size,
    float depth,
    int id,
    float x,
    float y,
    float z,
    float r,
    float g,
    float b,
    float a
)
    : Shape(id, x, y, z, r, g, b, a),
      text_(text),
      font_path_(font_path),
      font_size_(font_size),
      depth_(depth) {
    font_info_ = std::make_unique<stbtt_fontinfo>();
    LoadFont(font_path_);
    GenerateMesh(text_, font_size_, depth_);
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
        std::cerr << "Failed to open font file: " << font_path << std::endl;
        return;
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

void Text::GenerateMesh(const std::string& text, float font_size, float depth) {
    if (font_buffer_.empty()) {
        return;
    }

    std::vector<float> vertices;
    float x_offset = 0.0f;

    float scale = stbtt_ScaleForPixelHeight(font_info_.get(), font_size);

    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(font_info_.get(), &ascent, &descent, &line_gap);
    ascent = ascent * scale;
    descent = descent * scale;

    for (char c : text) {
        int glyph_index = stbtt_FindGlyphIndex(font_info_.get(), c);

        stbtt_vertex* stb_vertices;
        int num_vertices = stbtt_GetGlyphShape(font_info_.get(), glyph_index, &stb_vertices);

        if (num_vertices > 0) {
            std::vector<std::vector<std::array<float, 2>>> polygon;
            std::vector<std::array<float, 2>> current_contour;

            for (int i = 0; i < num_vertices; ++i) {
                stbtt_vertex v = stb_vertices[i];
                current_contour.push_back({ (v.x * scale) + x_offset, v.y * scale + ascent });

                if (i < num_vertices - 1 && stb_vertices[i+1].type == STBTT_vmove) {
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
                vertices.insert(vertices.end(), { v1[0], v1[1], depth / 2.0f, 0.0f, 0.0f, 1.0f });
                vertices.insert(vertices.end(), { v2[0], v2[1], depth / 2.0f, 0.0f, 0.0f, 1.0f });
                vertices.insert(vertices.end(), { v3[0], v3[1], depth / 2.0f, 0.0f, 0.0f, 1.0f });

                // Back face (winding order reversed for correct normal)
                vertices.insert(vertices.end(), { v1[0], v1[1], -depth / 2.0f, 0.0f, 0.0f, -1.0f });
                vertices.insert(vertices.end(), { v3[0], v3[1], -depth / 2.0f, 0.0f, 0.0f, -1.0f });
                vertices.insert(vertices.end(), { v2[0], v2[1], -depth / 2.0f, 0.0f, 0.0f, -1.0f });
            }

            for (const auto& contour : polygon) {
                for (size_t i = 0; i < contour.size(); ++i) {
                    size_t next_i = (i + 1) % contour.size();
                    glm::vec3 p1_front = { contour[i][0], contour[i][1], depth / 2.0f };
                    glm::vec3 p2_front = { contour[next_i][0], contour[next_i][1], depth / 2.0f };
                    glm::vec3 p1_back = { contour[i][0], contour[i][1], -depth / 2.0f };
                    glm::vec3 p2_back = { contour[next_i][0], contour[next_i][1], -depth / 2.0f };

                    glm::vec3 normal = glm::normalize(glm::cross(p2_front - p1_front, p1_back - p1_front));

                    vertices.insert(vertices.end(), { p1_front.x, p1_front.y, p1_front.z, normal.x, normal.y, normal.z });
                    vertices.insert(vertices.end(), { p2_front.x, p2_front.y, p2_front.z, normal.x, normal.y, normal.z });
                    vertices.insert(vertices.end(), { p1_back.x, p1_back.y, p1_back.z, normal.x, normal.y, normal.z });

                    vertices.insert(vertices.end(), { p1_back.x, p1_back.y, p1_back.z, normal.x, normal.y, normal.z });
                    vertices.insert(vertices.end(), { p2_front.x, p2_front.y, p2_front.z, normal.x, normal.y, normal.z });
                    vertices.insert(vertices.end(), { p2_back.x, p2_back.y, p2_back.z, normal.x, normal.y, normal.z });
                }
            }

            stbtt_FreeShape(font_info_.get(), stb_vertices);
        }

        int advance_width, left_side_bearing;
        stbtt_GetGlyphHMetrics(font_info_.get(), glyph_index, &advance_width, &left_side_bearing);
        x_offset += advance_width * scale;
    }

    vertex_count_ = vertices.size() / 6;

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

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
