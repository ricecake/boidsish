#include "procedural_head.h"
#include "shader.h"

#include <iostream>
#include <vector>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>


namespace Boidsish {

ProceduralHead::ProceduralHead() {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    generate_head_mesh(vertices, indices);
    original_vertices_ = vertices;
    mesh_ = std::make_unique<Mesh>(vertices, indices);
}

void ProceduralHead::render() const {
    if (!shader) {
        std::cerr << "ProceduralHead::render - Shader is not set!" << std::endl;
        return;
    }

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
    model *= glm::mat4_cast(GetRotation());
    model = glm::scale(model, GetScale());
    shader->setMat4("model", model);
    shader->setVec3("objectColor", glm::vec3(0.8f, 0.8f, 0.8f));

    mesh_->render();
}

namespace {
    // Facial regions - refined for the new head shape
    const glm::vec3 CHIN_CENTER(0.0f, -0.75f, 0.05f);
    const float CHIN_RADIUS = 0.25f;

    const glm::vec3 LEFT_EYE_CENTER(-0.4f, 0.4f, 0.7f);
    const glm::vec3 RIGHT_EYE_CENTER(0.4f, 0.4f, 0.7f);
    const float EYE_RADIUS = 0.15f;

    const glm::vec3 NOSE_CENTER(0.0f, 0.0f, 0.8f);
    const float NOSE_RADIUS = 0.2f;

    const glm::vec3 LEFT_CHEEK_CENTER(-0.6f, -0.2f, 0.3f);
    const glm::vec3 RIGHT_CHEEK_CENTER(0.6f, -0.2f, 0.3f);
    const float CHEEK_RADIUS = 0.3f;

    const glm::vec3 LEFT_EAR_CENTER(-0.9f, 0.0f, 0.0f);
    const glm::vec3 RIGHT_EAR_CENTER(0.9f, 0.0f, 0.0f);
    const float EAR_RADIUS = 0.3f;

    const glm::vec3 BROW_CENTER(0.0f, 0.7f, 0.4f);
    const float BROW_RADIUS = 0.4f;


    float calculate_falloff(float distance, float radius) {
        return glm::smoothstep(radius, 0.0f, distance);
    }
}

void ProceduralHead::deform_mesh() {
    if (!mesh_) return;

    auto& vertices = mesh_->vertices;

    for (size_t i = 0; i < vertices.size(); ++i) {
        glm::vec3& original_pos = original_vertices_[i].Position;
        glm::vec3& current_pos = vertices[i].Position;

        current_pos = original_pos;

        // --- Chin Size ---
        float chin_dist = glm::distance(original_pos, CHIN_CENTER);
        float chin_falloff = calculate_falloff(chin_dist, CHIN_RADIUS);
        current_pos.y *= 1.0f + (chin_size - 1.0f) * chin_falloff;

        // --- Brow Height & Width ---
        float brow_dist = glm::distance(original_pos, BROW_CENTER);
        float brow_falloff = calculate_falloff(brow_dist, BROW_RADIUS);
        current_pos.y += brow_height * brow_falloff;
        current_pos.x *= 1.0f + (brow_width - 1.0f) * brow_falloff;

        // --- Left Eye Separation & Size ---
        float left_eye_dist = glm::distance(original_pos, LEFT_EYE_CENTER);
        float left_eye_falloff = calculate_falloff(left_eye_dist, EYE_RADIUS);
        current_pos.x -= eye_separation * left_eye_falloff;
        current_pos += (original_pos - LEFT_EYE_CENTER) * (eye_size - 1.0f) * left_eye_falloff;

        // --- Right Eye Separation & Size ---
        float right_eye_dist = glm::distance(original_pos, RIGHT_EYE_CENTER);
        float right_eye_falloff = calculate_falloff(right_eye_dist, EYE_RADIUS);
        current_pos.x += eye_separation * right_eye_falloff;
        current_pos += (original_pos - RIGHT_EYE_CENTER) * (eye_size - 1.0f) * right_eye_falloff;

        // --- Nose Length & Size ---
        float nose_dist = glm::distance(original_pos, NOSE_CENTER);
        float nose_falloff = calculate_falloff(nose_dist, NOSE_RADIUS);
        current_pos.z += nose_length * nose_falloff;
        current_pos += (original_pos - NOSE_CENTER) * (nose_size - 1.0f) * nose_falloff;

        // --- Left Cheek Depth ---
        float left_cheek_dist = glm::distance(original_pos, LEFT_CHEEK_CENTER);
        float left_cheek_falloff = calculate_falloff(left_cheek_dist, CHEEK_RADIUS);
        current_pos.z += cheek_depth * left_cheek_falloff;

        // --- Right Cheek Depth ---
        float right_cheek_dist = glm::distance(original_pos, RIGHT_CHEEK_CENTER);
        float right_cheek_falloff = calculate_falloff(right_cheek_dist, CHEEK_RADIUS);
        current_pos.z += cheek_depth * right_cheek_falloff;

        // --- Left Ear Height ---
        float left_ear_dist = glm::distance(original_pos, LEFT_EAR_CENTER);
        float left_ear_falloff = calculate_falloff(left_ear_dist, EAR_RADIUS);
        current_pos.y += ear_height * left_ear_falloff;

        // --- Right Ear Height ---
        float right_ear_dist = glm::distance(original_pos, RIGHT_EAR_CENTER);
        float right_ear_falloff = calculate_falloff(right_ear_dist, EAR_RADIUS);
        current_pos.y += ear_height * right_ear_falloff;
    }

        // After deforming, recalculate normals
    for (size_t i = 0; i < vertices.size(); ++i) {
        vertices[i].Normal = glm::vec3(0.0f);
    }

    for (size_t i = 0; i < mesh_->indices.size(); i += 3) {
        Vertex& v1 = vertices[mesh_->indices[i]];
        Vertex& v2 = vertices[mesh_->indices[i + 1]];
        Vertex& v3 = vertices[mesh_->indices[i + 2]];

        glm::vec3 edge1 = v2.Position - v1.Position;
        glm::vec3 edge2 = v3.Position - v1.Position;
        glm::vec3 face_normal = glm::normalize(glm::cross(edge1, edge2));

        v1.Normal += face_normal;
        v2.Normal += face_normal;
        v3.Normal += face_normal;
    }

    for (size_t i = 0; i < vertices.size(); ++i) {
        vertices[i].Normal = glm::normalize(vertices[i].Normal);
    }

    // After deforming, update the buffers on the GPU
    update_buffers();
}

void ProceduralHead::update_buffers() {
    if (!mesh_) return;

    mesh_->updateGpuData();
}

void ProceduralHead::generate_head_mesh(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices) {
    const unsigned int X_SEGMENTS = 64;
    const unsigned int Y_SEGMENTS = 64;
    const float PI = 3.14159265359f;

    for (unsigned int y = 0; y <= Y_SEGMENTS; ++y) {
        for (unsigned int x = 0; x <= X_SEGMENTS; ++x) {
            float xSegment = (float)x / (float)X_SEGMENTS;
            float ySegment = (float)y / (float)Y_SEGMENTS;

            // Basic sphere coordinates
            float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
            float yPos = std::cos(ySegment * PI);
            float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

            // --- Head Shape Modifications ---
            // 1. Make it egg-shaped (wider at the top, narrower at the bottom)
            float taper_factor = (-yPos + 1.0f) / 2.0f; // 0 at top, 1 at bottom
            float head_shape_factor = 1.0f - 0.2f * taper_factor; // 1.0 at top, 0.8 at bottom
            xPos *= head_shape_factor;
            zPos *= head_shape_factor;


            // 2. Squash it slightly on the sides to make it less round
            xPos *= 0.9f;

            Vertex vertex;
            vertex.Position = glm::vec3(xPos, yPos, zPos);
            // Normals will be recalculated later, but start with something reasonable
            vertex.Normal = glm::normalize(glm::vec3(xPos, yPos, zPos));
            vertex.TexCoords = glm::vec2(xSegment, ySegment);
            vertices.push_back(vertex);
        }
    }

    // Same indexing as the sphere
    for (unsigned int y = 0; y < Y_SEGMENTS; ++y) {
        for (unsigned int x = 0; x < X_SEGMENTS; ++x) {
            indices.push_back(y * (X_SEGMENTS + 1) + x);
            indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
            indices.push_back((y + 1) * (X_SEGMENTS + 1) + x + 1);
            indices.push_back(y * (X_SEGMENTS + 1) + x);
            indices.push_back((y + 1) * (X_SEGMENTS + 1) + x + 1);
            indices.push_back(y * (X_SEGMENTS + 1) + x + 1);
        }
    }
}

} // namespace Boidsish
