#pragma once

#include "shape.h"
#include <vector>
#include <string>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/glm.hpp>

class Shader;

namespace Boidsish {

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

class Mesh {
public:
    // Mesh Data
    std::vector<Vertex>       vertices;
    std::vector<unsigned int> indices;
    unsigned int VAO;

    // Constructor
    Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices);

    // Render the mesh
    void render() const;

private:
    // Render data
    unsigned int VBO, EBO;

    // Initializes all the buffer objects/arrays
    void setupMesh();
};


class Model : public Shape {
public:
    // Constructor, expects a filepath to a 3D model.
    Model(const std::string& path);

    // Render the model
    void render() const override;

    const std::vector<Mesh>& getMeshes() const { return meshes; }

private:
    // Model data
    std::vector<Mesh> meshes;
    std::string directory;

    // Loads a model with supported ASSIMP extensions from file and stores the resulting meshes in meshes vector.
    void loadModel(const std::string& path);

    // Processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
    void processNode(aiNode *node, const aiScene *scene);
    Mesh processMesh(aiMesh *mesh, const aiScene *scene);
};

} // namespace Boidsish
