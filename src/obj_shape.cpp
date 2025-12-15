#include "obj_shape.h"
#include <cinolib/io/read_OBJ.h>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include "shader.h"

namespace Boidsish {

std::map<std::string, ObjShape::Mesh> ObjShape::mesh_cache_;

ObjShape::ObjShape(
    const std::string& obj_filepath,
    int id,
    float x,
    float y,
    float z,
    float size,
    float r,
    float g,
    float b,
    float a
) : Shape(id, x, y, z, r, g, b, a), size_(size) {
    mesh_ = LoadMesh(obj_filepath);
}

ObjShape::~ObjShape() {
}

void ObjShape::render() const {
    if (!mesh_) return;

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
    model = glm::scale(model, glm::vec3(size_));
    shader->setMat4("model", model);
    shader->setVec3("objectColor", GetR(), GetG(), GetB());

    glBindVertexArray(mesh_->vao);
    glDrawArrays(GL_TRIANGLES, 0, mesh_->vertex_count);
    glBindVertexArray(0);
}

const ObjShape::Mesh* ObjShape::LoadMesh(const std::string& filepath) {
    auto it = mesh_cache_.find(filepath);
    if (it != mesh_cache_.end()) {
        return &it->second;
    }

    std::vector<cinolib::vec3d> verts;
    std::vector<std::vector<uint>> polys;
    cinolib::read_OBJ(filepath.c_str(), verts, polys);
    if (verts.empty()) {
        std::cerr << "Failed to read OBJ file or file is empty: " << filepath << std::endl;
        return nullptr;
    }
    cinolib::Trimesh<> m(verts, polys);
    m.update_p_normals();
    m.update_v_normals();

    std::vector<float> vertices;
    const auto& vert_normals = m.vector_vert_normals();
    for (uint i = 0; i < m.num_polys(); ++i) {
        for (uint j = 0; j < 3; ++j) {
            uint vid = m.poly_vert_id(i, j);
            const cinolib::vec3d& v = m.vert(vid);
            const cinolib::vec3d& n = vert_normals[vid];
            vertices.push_back(v.x());
            vertices.push_back(v.y());
            vertices.push_back(v.z());
            vertices.push_back(n.x());
            vertices.push_back(n.y());
            vertices.push_back(n.z());
        }
    }

    Mesh new_mesh;
    new_mesh.vertex_count = vertices.size() / 6;

    glGenVertexArrays(1, &new_mesh.vao);
    glGenBuffers(1, &new_mesh.vbo);

    glBindVertexArray(new_mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, new_mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    mesh_cache_[filepath] = new_mesh;
    return &mesh_cache_[filepath];
}

void ObjShape::CleanUp() {
    for (auto const& [key, val] : mesh_cache_) {
        glDeleteVertexArrays(1, &val.vao);
        glDeleteBuffers(1, &val.vbo);
    }
}

} // namespace Boidsish
