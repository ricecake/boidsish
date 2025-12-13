#include "platonic_solid.h"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <map>
#include <vector>

#include "shader.h"

namespace {

struct SolidBuffers {
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    int vertex_count;
};

std::map<PlatonicSolidType, SolidBuffers> buffers;

// Precomputed vertex and normal data for platonic solids
// Data is interleaved: 3 floats for position, 3 floats for normal

// Tetrahedron
const GLfloat tetrahedron_vertices[] = {
    // position           // normal
    1.0f, 1.0f, 1.0f,     0.577f, 0.577f, 0.577f,
    -1.0f, -1.0f, 1.0f,    0.577f, 0.577f, 0.577f,
    -1.0f, 1.0f, -1.0f,    0.577f, 0.577f, 0.577f,

    1.0f, 1.0f, 1.0f,     0.577f, -0.577f, -0.577f,
    -1.0f, -1.0f, 1.0f,    0.577f, -0.577f, -0.577f,
    1.0f, -1.0f, -1.0f,    0.577f, -0.577f, -0.577f,

    1.0f, 1.0f, 1.0f,     -0.577f, 0.577f, -0.577f,
    -1.0f, 1.0f, -1.0f,    -0.577f, 0.577f, -0.577f,
    1.0f, -1.0f, -1.0f,    -0.577f, 0.577f, -0.577f,

    -1.0f, -1.0f, 1.0f,    -0.577f, -0.577f, 0.577f,
    -1.0f, 1.0f, -1.0f,    -0.577f, -0.577f, 0.577f,
    1.0f, -1.0f, -1.0f,    -0.577f, -0.577f, 0.577f,
};

// Cube
const GLfloat cube_vertices[] = {
    // position           // normal
    -1.0f,-1.0f,-1.0f,  0.0f, 0.0f,-1.0f,
     1.0f,-1.0f,-1.0f,  0.0f, 0.0f,-1.0f,
     1.0f, 1.0f,-1.0f,  0.0f, 0.0f,-1.0f,
     1.0f, 1.0f,-1.0f,  0.0f, 0.0f,-1.0f,
    -1.0f, 1.0f,-1.0f,  0.0f, 0.0f,-1.0f,
    -1.0f,-1.0f,-1.0f,  0.0f, 0.0f,-1.0f,

    -1.0f,-1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
     1.0f,-1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
     1.0f, 1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
     1.0f, 1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
    -1.0f, 1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
    -1.0f,-1.0f, 1.0f,  0.0f, 0.0f, 1.0f,

    -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f,
    -1.0f, 1.0f,-1.0f, -1.0f, 0.0f, 0.0f,
    -1.0f,-1.0f,-1.0f, -1.0f, 0.0f, 0.0f,
    -1.0f,-1.0f,-1.0f, -1.0f, 0.0f, 0.0f,
    -1.0f,-1.0f, 1.0f, -1.0f, 0.0f, 0.0f,
    -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f,

     1.0f, 1.0f, 1.0f,  1.0f, 0.0f, 0.0f,
     1.0f, 1.0f,-1.0f,  1.0f, 0.0f, 0.0f,
     1.0f,-1.0f,-1.0f,  1.0f, 0.0f, 0.0f,
     1.0f,-1.0f,-1.0f,  1.0f, 0.0f, 0.0f,
     1.0f,-1.0f, 1.0f,  1.0f, 0.0f, 0.0f,
     1.0f, 1.0f, 1.0f,  1.0f, 0.0f, 0.0f,

    -1.0f,-1.0f,-1.0f,  0.0f,-1.0f, 0.0f,
     1.0f,-1.0f,-1.0f,  0.0f,-1.0f, 0.0f,
     1.0f,-1.0f, 1.0f,  0.0f,-1.0f, 0.0f,
     1.0f,-1.0f, 1.0f,  0.0f,-1.0f, 0.0f,
    -1.0f,-1.0f, 1.0f,  0.0f,-1.0f, 0.0f,
    -1.0f,-1.0f,-1.0f,  0.0f,-1.0f, 0.0f,

    -1.0f, 1.0f,-1.0f,  0.0f, 1.0f, 0.0f,
     1.0f, 1.0f,-1.0f,  0.0f, 1.0f, 0.0f,
     1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 0.0f,
     1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 0.0f,
    -1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 0.0f,
    -1.0f, 1.0f,-1.0f,  0.0f, 1.0f, 0.0f
};

// Octahedron
const GLfloat octahedron_vertices[] = {
    // position           // normal
     1.0f, 0.0f, 0.0f,    0.577f, 0.577f, 0.577f,
     0.0f, 1.0f, 0.0f,    0.577f, 0.577f, 0.577f,
     0.0f, 0.0f, 1.0f,    0.577f, 0.577f, 0.577f,

     1.0f, 0.0f, 0.0f,    0.577f, 0.577f, -0.577f,
     0.0f, 1.0f, 0.0f,    0.577f, 0.577f, -0.577f,
     0.0f, 0.0f,-1.0f,    0.577f, 0.577f, -0.577f,

     1.0f, 0.0f, 0.0f,    0.577f, -0.577f, 0.577f,
     0.0f,-1.0f, 0.0f,    0.577f, -0.577f, 0.577f,
     0.0f, 0.0f, 1.0f,    0.577f, -0.577f, 0.577f,

     1.0f, 0.0f, 0.0f,    0.577f, -0.577f, -0.577f,
     0.0f,-1.0f, 0.0f,    0.577f, -0.577f, -0.577f,
     0.0f, 0.0f,-1.0f,    0.577f, -0.577f, -0.577f,

    -1.0f, 0.0f, 0.0f,   -0.577f, 0.577f, 0.577f,
     0.0f, 1.0f, 0.0f,   -0.577f, 0.577f, 0.577f,
     0.0f, 0.0f, 1.0f,   -0.577f, 0.577f, 0.577f,

    -1.0f, 0.0f, 0.0f,   -0.577f, 0.577f, -0.577f,
     0.0f, 1.0f, 0.0f,   -0.577f, 0.577f, -0.577f,
     0.0f, 0.0f,-1.0f,   -0.577f, 0.577f, -0.577f,

    -1.0f, 0.0f, 0.0f,   -0.577f, -0.577f, 0.577f,
     0.0f,-1.0f, 0.0f,   -0.577f, -0.577f, 0.577f,
     0.0f, 0.0f, 1.0f,   -0.577f, -0.577f, 0.577f,

    -1.0f, 0.0f, 0.0f,   -0.577f, -0.577f, -0.577f,
     0.0f,-1.0f, 0.0f,   -0.577f, -0.577f, -0.577f,
     0.0f, 0.0f,-1.0f,   -0.577f, -0.577f, -0.577f,
};

// Dodecahedron
// Data from https://www.geeks3d.com/20141201/how-to-render-a-basic-platonic-solid-in-opengl-c-or-glsl/
const float G = 1.61803398875f;
const float G_1 = 0.61803398875f;
const GLfloat dodecahedron_vertices_data[20][3] = {
  {-1, -1, -1}, {-1, -1, 1}, {-1, 1, -1}, {-1, 1, 1},
  {1, -1, -1}, {1, -1, 1}, {1, 1, -1}, {1, 1, 1},
  {0, -G_1, -G}, {0, -G_1, G}, {0, G_1, -G}, {0, G_1, G},
  {-G_1, -G, 0}, {-G_1, G, 0}, {G_1, -G, 0}, {G_1, G, 0},
  {-G, 0, -G_1}, {-G, 0, G_1}, {G, 0, -G_1}, {G, 0, G_1}
};

const GLuint dodecahedron_triangle_indices[36][3] = {
  {2, 10, 13}, {2, 13, 3}, {10, 6, 15}, {10, 15, 11}, {13, 1, 17}, {13, 17, 3},
  {16, 2, 0}, {16, 0, 8}, {6, 10, 2}, {6, 2, 16}, {15, 13, 11}, {15, 11, 7},
  {1, 12, 9}, {1, 9, 17}, {18, 16, 8}, {18, 8, 4}, {19, 7, 11}, {19, 11, 3},
  {5, 14, 9}, {5, 9, 12}, {18, 6, 16}, {18, 6, 19}, {4, 14, 5}, {4, 5, 8},
  {7, 19, 6}, {7, 6, 15}, {17, 9, 11}, {17, 11, 3}, {12, 8, 14}, {12, 8, 0},
  {1, 0, 12}, {1, 0, 16}, {9, 5, 11}, {9, 5, 14}, {18, 4, 19}, {18, 4, 8},
};

// Icosahedron
#define X .525731112119133606f
#define Z .850650808352039932f

static const GLfloat icosahedron_vertices[12][3] = {
   {-X, 0.0, Z}, {X, 0.0, Z}, {-X, 0.0, -Z}, {X, 0.0, -Z},
   {0.0, Z, X}, {0.0, Z, -X}, {0.0, -Z, X}, {0.0, -Z, -X},
   {Z, X, 0.0}, {-Z, X, 0.0}, {Z, -X, 0.0}, {-Z, -X, 0.0}
};

static const GLuint icosahedron_indices[20][3] = {
   {0,4,1}, {0,9,4}, {9,5,4}, {4,5,8}, {4,8,1},
   {8,10,1}, {8,3,10}, {5,3,8}, {5,2,3}, {2,7,3},
   {7,10,3}, {7,6,10}, {7,11,6}, {11,0,6}, {0,1,6},
   {6,1,10}, {9,0,11}, {9,11,2}, {9,2,5}, {7,2,11}
};

void create_solid_buffers(PlatonicSolidType type, const GLfloat* vertices, int size) {
    SolidBuffers b;
    b.vertex_count = size / (6 * sizeof(GLfloat));
    b.ebo = 0;

    glGenVertexArrays(1, &b.vao);
    glGenBuffers(1, &b.vbo);

    glBindVertexArray(b.vao);
    glBindBuffer(GL_ARRAY_BUFFER, b.vbo);
    glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    buffers[type] = b;
}

} // anonymous namespace

PlatonicSolid::PlatonicSolid(PlatonicSolidType type, int id, float x, float y, float z,
                             float r, float g, float b, float a,
                             int trail_length)
    : Shape(id, x, y, z, r, g, b, a, trail_length), type_(type) {}

void PlatonicSolid::Init() {
    create_solid_buffers(PlatonicSolidType::TETRAHEDRON, tetrahedron_vertices, sizeof(tetrahedron_vertices));
    create_solid_buffers(PlatonicSolidType::CUBE, cube_vertices, sizeof(cube_vertices));
    create_solid_buffers(PlatonicSolidType::OCTAHEDRON, octahedron_vertices, sizeof(octahedron_vertices));

    // Dodecahedron
    std::vector<GLfloat> dodecahedron_v;
    for (int i = 0; i < 36; ++i) { // For each of the 36 triangles
        // Get the three vertices of the triangle to calculate the face normal
        glm::vec3 v0(dodecahedron_vertices_data[dodecahedron_triangle_indices[i][0]][0],
                     dodecahedron_vertices_data[dodecahedron_triangle_indices[i][0]][1],
                     dodecahedron_vertices_data[dodecahedron_triangle_indices[i][0]][2]);
        glm::vec3 v1(dodecahedron_vertices_data[dodecahedron_triangle_indices[i][1]][0],
                     dodecahedron_vertices_data[dodecahedron_triangle_indices[i][1]][1],
                     dodecahedron_vertices_data[dodecahedron_triangle_indices[i][1]][2]);
        glm::vec3 v2(dodecahedron_vertices_data[dodecahedron_triangle_indices[i][2]][0],
                     dodecahedron_vertices_data[dodecahedron_triangle_indices[i][2]][1],
                     dodecahedron_vertices_data[dodecahedron_triangle_indices[i][2]][2]);

        glm::vec3 normal = glm::normalize(glm::cross(v2 - v0, v1 - v0));

        auto push_vertex = [&](int index) {
            dodecahedron_v.push_back(dodecahedron_vertices_data[index][0]);
            dodecahedron_v.push_back(dodecahedron_vertices_data[index][1]);
            dodecahedron_v.push_back(dodecahedron_vertices_data[index][2]);
            dodecahedron_v.push_back(normal.x);
            dodecahedron_v.push_back(normal.y);
            dodecahedron_v.push_back(normal.z);
        };

        push_vertex(dodecahedron_triangle_indices[i][0]);
        push_vertex(dodecahedron_triangle_indices[i][1]);
        push_vertex(dodecahedron_triangle_indices[i][2]);
    }
    create_solid_buffers(PlatonicSolidType::DODECAHEDRON, dodecahedron_v.data(), dodecahedron_v.size() * sizeof(GLfloat));

    // Icosahedron
    std::vector<GLfloat> icosahedron_v;
    for (int i = 0; i < 20; ++i) { // For each of the 20 triangular faces
        glm::vec3 v0(icosahedron_vertices[icosahedron_indices[i][0]][0],
                     icosahedron_vertices[icosahedron_indices[i][0]][1],
                     icosahedron_vertices[icosahedron_indices[i][0]][2]);
        glm::vec3 v1(icosahedron_vertices[icosahedron_indices[i][1]][0],
                     icosahedron_vertices[icosahedron_indices[i][1]][1],
                     icosahedron_vertices[icosahedron_indices[i][1]][2]);
        glm::vec3 v2(icosahedron_vertices[icosahedron_indices[i][2]][0],
                     icosahedron_vertices[icosahedron_indices[i][2]][1],
                     icosahedron_vertices[icosahedron_indices[i][2]][2]);

        glm::vec3 normal = glm::normalize(glm::cross(v2 - v0, v1 - v0));

        // Vertex 1
        icosahedron_v.push_back(v0.x);
        icosahedron_v.push_back(v0.y);
        icosahedron_v.push_back(v0.z);
        icosahedron_v.push_back(normal.x);
        icosahedron_v.push_back(normal.y);
        icosahedron_v.push_back(normal.z);

        // Vertex 2
        icosahedron_v.push_back(v1.x);
        icosahedron_v.push_back(v1.y);
        icosahedron_v.push_back(v1.z);
        icosahedron_v.push_back(normal.x);
        icosahedron_v.push_back(normal.y);
        icosahedron_v.push_back(normal.z);

        // Vertex 3
        icosahedron_v.push_back(v2.x);
        icosahedron_v.push_back(v2.y);
        icosahedron_v.push_back(v2.z);
        icosahedron_v.push_back(normal.x);
        icosahedron_v.push_back(normal.y);
        icosahedron_v.push_back(normal.z);
    }
    create_solid_buffers(PlatonicSolidType::ICOSAHEDRON, icosahedron_v.data(), icosahedron_v.size() * sizeof(GLfloat));
}

void PlatonicSolid::Cleanup() {
    for (auto const& [type, b] : buffers) {
        glDeleteVertexArrays(1, &b.vao);
        glDeleteBuffers(1, &b.vbo);
        if (b.ebo) {
            glDeleteBuffers(1, &b.ebo);
        }
    }
    buffers.clear();
}

void PlatonicSolid::render() const {
    if (buffers.find(type_) == buffers.end()) {
        return; // Or some error handling
    }

    const auto& b = buffers.at(type_);

    shader->use();
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
    shader->setMat4("model", model);
    shader->setVec3("objectColor", GetR(), GetG(), GetB());
    shader->setInt("useVertexColor", 0);

    glBindVertexArray(b.vao);
    if (b.ebo) {
        glDrawElements(GL_TRIANGLES, b.vertex_count, GL_UNSIGNED_INT, 0);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, b.vertex_count);
    }
    glBindVertexArray(0);
}
