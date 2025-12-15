#pragma once

#include "shape.h"
#include <cinolib/meshes/trimesh.h>
#include <string>
#include <vector>
#include <map>
#include <GL/glew.h>

namespace Boidsish {

class ObjShape : public Shape {
public:
    ObjShape(
        const std::string& obj_filepath,
        int id = 0,
        float x = 0.0f,
        float y = 0.0f,
        float z = 0.0f,
        float size = 1.0f,
        float r = 1.0f,
        float g = 1.0f,
        float b = 1.0f,
        float a = 1.0f
    );

    ~ObjShape();

    void render() const override;

    inline float GetSize() const { return size_; }
    inline void SetSize(float size) { size_ = size; }

private:
    struct Mesh {
        GLuint vao;
        GLuint vbo;
        int vertex_count;
    };

    static std::map<std::string, Mesh> mesh_cache_;
    const Mesh* mesh_;
    float size_;

    static const Mesh* LoadMesh(const std::string& filepath);

public:
    static void CleanUp();
};

} // namespace Boidsish
