#include "graph.h"
#include "shader.h"
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>
#include <vector>
#include <array>

namespace Boidsish {

Graph::Graph(int id, float x, float y, float z) : Shape(id, x, y, z, 1.0f, 1.0f, 1.0f, 1.0f, 0) {
    this->id = id;
    this->x = x;
    this->y = y;
    this->z = z;
    this->r = 1.0f;
    this->g = 1.0f;
    this->b = 1.0f;
    this->a = 1.0f;
    this->trail_length = 0;
}

Graph::~Graph() {
    if (edge_VAO != 0) {
        glDeleteVertexArrays(1, &edge_VAO);
        glDeleteBuffers(1, &edge_VBO);
    }
}

void Graph::setVertices(const std::vector<Vertex>& new_vertices) {
    vertices = new_vertices;
    buffers_dirty = true;
}

const int SPHERE_LONGITUDE_SEGMENTS = 12;
const int SPHERE_LATITUDE_SEGMENTS = 8;
const float SPHERE_RADIUS_SCALE = 0.01f;
const int CYLINDER_SEGMENTS = 12;
const float EDGE_RADIUS_SCALE = 0.005f;
const int CURVE_SEGMENTS = 10;

Vector3 CatmullRom(float t, const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3) {
    return 0.5f *
           ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * (t * t) +
            (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * (t * t * t));
}

void Graph::render() const {
    // This is a dummy implementation to satisfy the base class.
    // The actual rendering is done in the other render method.
}

void Graph::updateEdgeBuffers(const Vector3& offset) const {
    if (edge_VAO == 0) {
        glGenVertexArrays(1, &edge_VAO);
        glGenBuffers(1, &edge_VBO);
    }

    std::vector<float> all_vertex_data;

    std::map<int, std::vector<int>> adj;
    for (const auto& edge : edges) {
        adj[edge.vertex1_idx].push_back(edge.vertex2_idx);
        adj[edge.vertex2_idx].push_back(edge.vertex1_idx);
    }

    for (const auto& edge : edges) {
        if (edge.vertex1_idx < 0 || edge.vertex1_idx >= (int)vertices.size() || edge.vertex2_idx < 0 ||
            edge.vertex2_idx >= (int)vertices.size()) {
            continue;
        }

        const auto& v1 = vertices[edge.vertex1_idx];
        const auto& v2 = vertices[edge.vertex2_idx];

        Vertex v0;
        int v0_idx = -1;
        if(adj.count(edge.vertex1_idx)) {
            for (int neighbor_idx : adj[edge.vertex1_idx]) {
                if (neighbor_idx != edge.vertex2_idx) {
                    v0_idx = neighbor_idx;
                    break;
                }
            }
        }
        if (v0_idx != -1) {
            v0 = vertices[v0_idx];
        } else {
            v0 = v1;
            v0.position = v1.position - (v2.position - v1.position);
        }

        Vertex v3;
        int v3_idx = -1;
        if(adj.count(edge.vertex2_idx)) {
            for (int neighbor_idx : adj[edge.vertex2_idx]) {
                if (neighbor_idx != edge.vertex1_idx) {
                    v3_idx = neighbor_idx;
                    break;
                }
            }
        }
        if (v3_idx != -1) {
            v3 = vertices[v3_idx];
        } else {
            v3 = v2;
            v3.position = v2.position + (v2.position - v1.position);
        }

        Vector3 p0 = v0.position + offset;
        Vector3 p1 = v1.position + offset;
        Vector3 p2 = v2.position + offset;
        Vector3 p3 = v3.position + offset;

        std::vector<Vector3> points;
        std::vector<std::array<float, 4>> colors;
        std::vector<float> radii;

        for (int i = 0; i <= CURVE_SEGMENTS; ++i) {
            float t = (float)i / CURVE_SEGMENTS;
            points.push_back(CatmullRom(t, p0, p1, p2, p3));

            float u = 1.0f - t;
            colors.push_back({u * v1.r + t * v2.r, u * v1.g + t * v2.g, u * v1.b + t * v2.b, u * v1.a + t * v2.a});
            radii.push_back((u * v1.size + t * v2.size) * EDGE_RADIUS_SCALE);
        }

        if (points.size() < 2) continue;

        std::vector<Vector3> tangents;
        for (size_t i = 0; i < points.size(); ++i) {
            if (i == 0)
                tangents.push_back((points[1] - points[0]).Normalized());
            else if (i == points.size() - 1)
                tangents.push_back((points[i] - points[i - 1]).Normalized());
            else
                tangents.push_back((points[i + 1] - points[i - 1]).Normalized());
        }

        Vector3 normal;
        if (std::abs(tangents[0].y) < 0.999f)
            normal = tangents[0].Cross(Vector3(0, 1, 0)).Normalized();
        else
            normal = tangents[0].Cross(Vector3(1, 0, 0)).Normalized();

        for (size_t i = 0; i < points.size() - 1; ++i) {
            Vector3 t_prev = tangents[i];
            Vector3 t_curr = tangents[i+1];
            Vector3 axis = t_prev.Cross(t_curr);
            float angle = acos(std::max(-1.0f, std::min(1.0f, t_prev.Dot(t_curr))));
            if (axis.MagnitudeSquared() > 1e-6) {
                float cos_a = cos(angle);
                float sin_a = sin(angle);
                axis.Normalize();
                normal = normal * cos_a + axis.Cross(normal) * sin_a + axis * axis.Dot(normal) * (1 - cos_a);
            }
            Vector3 bitangent = tangents[i+1].Cross(normal).Normalized();

            for (int j = 0; j <= CYLINDER_SEGMENTS; ++j) {
                float angle_j = 2.0f * M_PI * (float)j / CYLINDER_SEGMENTS;
                Vector3 circle_normal = (normal * cos(angle_j) + bitangent * sin(angle_j)).Normalized();

                all_vertex_data.push_back(points[i].x + circle_normal.x * radii[i]);
                all_vertex_data.push_back(points[i].y + circle_normal.y * radii[i]);
                all_vertex_data.push_back(points[i].z + circle_normal.z * radii[i]);
                all_vertex_data.push_back(circle_normal.x);
                all_vertex_data.push_back(circle_normal.y);
                all_vertex_data.push_back(circle_normal.z);
                all_vertex_data.push_back(colors[i][0]);
                all_vertex_data.push_back(colors[i][1]);
                all_vertex_data.push_back(colors[i][2]);
                all_vertex_data.push_back(colors[i][3]);

                all_vertex_data.push_back(points[i+1].x + circle_normal.x * radii[i+1]);
                all_vertex_data.push_back(points[i+1].y + circle_normal.y * radii[i+1]);
                all_vertex_data.push_back(points[i+1].z + circle_normal.z * radii[i+1]);
                all_vertex_data.push_back(circle_normal.x);
                all_vertex_data.push_back(circle_normal.y);
                all_vertex_data.push_back(circle_normal.z);
                all_vertex_data.push_back(colors[i+1][0]);
                all_vertex_data.push_back(colors[i+1][1]);
                all_vertex_data.push_back(colors[i+1][2]);
                all_vertex_data.push_back(colors[i+1][3]);
            }
        }
    }

    edge_vertex_count = all_vertex_data.size() / 10;

    glBindVertexArray(edge_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, edge_VBO);
    glBufferData(GL_ARRAY_BUFFER, all_vertex_data.size() * sizeof(float), all_vertex_data.data(), GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    buffers_dirty = false;
}

void Graph::render(Shader& sphere_shader, Shader& edge_shader, unsigned int sphere_VAO, int sphere_index_count, const glm::mat4& projection, const glm::mat4& view) const {
    Vector3 offset(x, y, z);

    // Render vertices
    sphere_shader.use();
    sphere_shader.setVec3("lightColor", 1.0f, 1.0f, 1.0f);
    sphere_shader.setVec3("lightPos", 1.0f, 1.0f, 1.0f);
    sphere_shader.setMat4("projection", projection);
    sphere_shader.setMat4("view", view);

    glBindVertexArray(sphere_VAO);
    for (const auto& vertex : vertices) {
        sphere_shader.setVec3("objectColor", vertex.r, vertex.g, vertex.b);
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(vertex.position.x + offset.x, vertex.position.y + offset.y, vertex.position.z + offset.z));
        model = glm::scale(model, glm::vec3(vertex.size * SPHERE_RADIUS_SCALE));
        sphere_shader.setMat4("model", model);
        glDrawElements(GL_TRIANGLES, sphere_index_count, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);

    // Render edges
    if (buffers_dirty) {
        updateEdgeBuffers(offset);
    }

    if (edge_vertex_count > 0) {
        edge_shader.use();
        edge_shader.setVec3("lightColor", 1.0f, 1.0f, 1.0f);
        edge_shader.setVec3("lightPos", 1.0f, 1.0f, 1.0f);
        edge_shader.setMat4("projection", projection);
        edge_shader.setMat4("view", view);

        glm::mat4 model = glm::mat4(1.0f);
        edge_shader.setMat4("model", model);

        glBindVertexArray(edge_VAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, edge_vertex_count);
        glBindVertexArray(0);
    }
}

} // namespace Boidsish
