#include "teapot.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "GL/glew.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "shader.h"

namespace Boidsish {

Teapot::Teapot(int id, float x, float y, float z) : Shape(id, x, y, z) {}

void Teapot::render() const {
  if (!initialized_) {
    create_buffers();
    initialized_ = true;
  }

  if (shader) {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
    model = glm::scale(model, glm::vec3(0.1f, 0.1f, 0.1f));

    shader->use();
    shader->setMat4("model", model);
    shader->setVec4("objectColor", GetR(), GetG(), GetB(), GetA());

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, num_vertices_);
    glBindVertexArray(0);
  }
}

void Teapot::create_buffers() const {
  std::ifstream obj_file("utah_teapot.obj");
  if (!obj_file.is_open()) {
    std::cerr << "Error opening utah_teapot.obj" << std::endl;
    return;
  }

  std::vector<glm::vec3> vertices;
  std::vector<glm::vec3> normals;
  std::vector<GLfloat> buffer_data;

  std::string line;
  std::vector<glm::vec3> temp_vertices;
  while (std::getline(obj_file, line)) {
    std::stringstream ss(line);
    std::string type;
    ss >> type;

    if (type == "v") {
      glm::vec3 vertex;
      ss >> vertex.x >> vertex.y >> vertex.z;
      temp_vertices.push_back(vertex);
    } else if (type == "f") {
      std::vector<int> face_indices;
      std::string face_vertex;
      while (ss >> face_vertex) {
        face_indices.push_back(std::stoi(face_vertex));
      }

      // Triangulate faces (assuming convex polygons)
      for (size_t i = 1; i < face_indices.size() - 1; ++i) {
        glm::vec3 v0 = temp_vertices[face_indices[0] - 1];
        glm::vec3 v1 = temp_vertices[face_indices[i] - 1];
        glm::vec3 v2 = temp_vertices[face_indices[i + 1] - 1];

        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);

        glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        normals.push_back(normal);
        normals.push_back(normal);
        normals.push_back(normal);
      }
    }
  }

  for (size_t i = 0; i < vertices.size(); ++i) {
    buffer_data.push_back(vertices[i].x);
    buffer_data.push_back(vertices[i].y);
    buffer_data.push_back(vertices[i].z);
    buffer_data.push_back(normals[i].x);
    buffer_data.push_back(normals[i].y);
    buffer_data.push_back(normals[i].z);
  }

  glGenVertexArrays(1, &vao_);
  glBindVertexArray(vao_);

  glGenBuffers(1, &vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, buffer_data.size() * sizeof(GLfloat),
               buffer_data.data(), GL_STATIC_DRAW);

  // Position
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat),
                        (void*)0);
  glEnableVertexAttribArray(0);

  // Normal
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat),
                        (void*)(3 * sizeof(GLfloat)));
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  num_vertices_ = vertices.size();
}
}
