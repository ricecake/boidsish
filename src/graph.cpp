#include "graph.h"
#include <GL/glew.h>
#include <cmath>

namespace Boidsish {

Graph::Graph(
    int   id,
    float x,
    float y,
    float z
) {
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

const int SPHERE_LONGITUDE_SEGMENTS = 12;
const int SPHERE_LATITUDE_SEGMENTS = 8;
const float SPHERE_RADIUS_SCALE = 0.01f;
const int CYLINDER_SEGMENTS = 12;
const float EDGE_RADIUS_SCALE = 0.005f;
const int CURVE_SEGMENTS = 10;

void render_sphere(const Graph::Vertex& vertex, const Vector3& offset) {
    float radius = vertex.size * SPHERE_RADIUS_SCALE;

    GLfloat material_ambient[] = {vertex.r * 0.2f, vertex.g * 0.2f, vertex.b * 0.2f, vertex.a};
    GLfloat material_diffuse[] = {vertex.r, vertex.g, vertex.b, vertex.a};
    GLfloat material_specular[] = {0.5f, 0.5f, 0.5f, vertex.a};
    GLfloat material_shininess[] = {32.0f};

    glMaterialfv(GL_FRONT, GL_AMBIENT, material_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, material_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, material_specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, material_shininess);

    glPushMatrix();
    glTranslatef(vertex.position.x + offset.x, vertex.position.y + offset.y, vertex.position.z + offset.z);

    glBegin(GL_TRIANGLES);

    for (int lat = 0; lat < SPHERE_LATITUDE_SEGMENTS; ++lat) {
        float lat0 = M_PI * (-0.5f + (float)lat / SPHERE_LATITUDE_SEGMENTS);
        float lat1 = M_PI * (-0.5f + (float)(lat + 1) / SPHERE_LATITUDE_SEGMENTS);

        float y0 = sin(lat0) * radius;
        float y1 = sin(lat1) * radius;
        float r0 = cos(lat0) * radius;
        float r1 = cos(lat1) * radius;

        for (int lon = 0; lon < SPHERE_LONGITUDE_SEGMENTS; ++lon) {
            float lon0 = 2 * M_PI * (float)lon / SPHERE_LONGITUDE_SEGMENTS;
            float lon1 = 2 * M_PI * (float)(lon + 1) / SPHERE_LONGITUDE_SEGMENTS;

            float x0 = cos(lon0);
            float z0 = sin(lon0);
            float x1 = cos(lon1);
            float z1 = sin(lon1);

            glNormal3f(x0 * cos(lat0), sin(lat0), z0 * cos(lat0));
            glVertex3f(x0 * r0, y0, z0 * r0);

            glNormal3f(x1 * cos(lat0), sin(lat0), z1 * cos(lat0));
            glVertex3f(x1 * r0, y0, z1 * r0);

            glNormal3f(x1 * cos(lat1), sin(lat1), z1 * cos(lat1));
            glVertex3f(x1 * r1, y1, z1 * r1);

            glNormal3f(x0 * cos(lat0), sin(lat0), z0 * cos(lat0));
            glVertex3f(x0 * r0, y0, z0 * r0);

            glNormal3f(x1 * cos(lat1), sin(lat1), z1 * cos(lat1));
            glVertex3f(x1 * r1, y1, z1 * r1);

            glNormal3f(x0 * cos(lat1), sin(lat1), z0 * cos(lat1));
            glVertex3f(x0 * r1, y1, z0 * r1);
        }
    }

    glEnd();
    glPopMatrix();
}

Vector3 CatmullRom(float t, const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3) {
    return 0.5f * (
        (2.0f * p1) +
        (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * (t * t) +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * (t * t * t)
    );
}

void render_edge(const Graph::Vertex& v0, const Graph::Vertex& v1, const Graph::Vertex& v2, const Graph::Vertex& v3, const Vector3& offset) {
    Vector3 p0 = v0.position + offset;
    Vector3 p1 = v1.position + offset;
    Vector3 p2 = v2.position + offset;
    Vector3 p3 = v3.position + offset;

    // 1. Pre-calculate all points, tangents, colors, and radii
    std::vector<Vector3> points;
    std::vector<Vector3> tangents;
    std::vector<std::array<float, 4>> colors;
    std::vector<float> radii;

    for (int i = 0; i <= CURVE_SEGMENTS; ++i) {
        float t = (float)i / CURVE_SEGMENTS;
        points.push_back(CatmullRom(t, p0, p1, p2, p3));

        float u = 1.0f - t;
        colors.push_back({u * v1.r + t * v2.r, u * v1.g + t * v2.g, u * v1.b + t * v2.b, u * v1.a + t * v2.a});
        radii.push_back((u * v1.size + t * v2.size) * EDGE_RADIUS_SCALE);
    }

    if (points.size() < 2) return;

    for(size_t i = 0; i < points.size(); ++i) {
        if (i == 0) tangents.push_back((points[1] - points[0]).Normalized());
        else if (i == points.size() - 1) tangents.push_back((points[i] - points[i-1]).Normalized());
        else tangents.push_back((points[i+1] - points[i-1]).Normalized());
    }

    // 2. Generate all vertices for the tube
    std::vector<std::vector<Vector3>> rings;
    std::vector<std::vector<Vector3>> ring_normals;

    Vector3 normal;
    if (std::abs(tangents[0].y) < 0.999f) normal = tangents[0].Cross(Vector3(0, 1, 0)).Normalized();
    else normal = tangents[0].Cross(Vector3(1, 0, 0)).Normalized();

    for (size_t i = 0; i < points.size(); ++i) {
        if (i > 0) {
            Vector3 t_prev = tangents[i-1];
            Vector3 t_curr = tangents[i];
            Vector3 axis = t_prev.Cross(t_curr);
            float angle = acos(std::max(-1.0f, std::min(1.0f, t_prev.Dot(t_curr))));
            if (axis.MagnitudeSquared() > 1e-6) {
                float cos_a = cos(angle);
                float sin_a = sin(angle);
                axis.Normalize();
                normal = normal * cos_a + axis.Cross(normal) * sin_a + axis * axis.Dot(normal) * (1 - cos_a);
            }
        }
        Vector3 bitangent = tangents[i].Cross(normal).Normalized();

        std::vector<Vector3> ring;
        std::vector<Vector3> normals_ring;
        for (int j = 0; j <= CYLINDER_SEGMENTS; ++j) {
            float angle = 2.0f * M_PI * (float)j / CYLINDER_SEGMENTS;
            Vector3 circle_normal = (normal * cos(angle) + bitangent * sin(angle)).Normalized();
            ring.push_back(points[i] + circle_normal * radii[i]);
            normals_ring.push_back(circle_normal);
        }
        rings.push_back(ring);
        ring_normals.push_back(normals_ring);
    }

    // 3. Render the tube using GL_TRIANGLE_STRIP
    for (size_t i = 0; i < points.size() - 1; ++i) {
        glBegin(GL_TRIANGLE_STRIP);
        for (int j = 0; j <= CYLINDER_SEGMENTS; ++j) {
            // Vertex from current ring
            glColor4fv(colors[i].data());
            glNormal3fv(&ring_normals[i][j].x);
            glVertex3fv(&rings[i][j].x);

            // Vertex from next ring
            glColor4fv(colors[i+1].data());
            glNormal3fv(&ring_normals[i+1][j].x);
            glVertex3fv(&rings[i+1][j].x);
        }
        glEnd();
    }
}

void Graph::render() const {
    Vector3 offset(x, y, z);
    for (const auto& vertex : vertices) {
        render_sphere(vertex, offset);
    }

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    std::map<int, std::vector<int>> adj;
    for (const auto& edge : edges) {
        adj[edge.vertex1_idx].push_back(edge.vertex2_idx);
        adj[edge.vertex2_idx].push_back(edge.vertex1_idx);
    }

    for (const auto& edge : edges) {
        if (edge.vertex1_idx < 0 || edge.vertex1_idx >= (int)vertices.size() ||
            edge.vertex2_idx < 0 || edge.vertex2_idx >= (int)vertices.size()) {
            continue;
        }

        const auto& v1 = vertices[edge.vertex1_idx];
        const auto& v2 = vertices[edge.vertex2_idx];

        Vertex v0;
        int v0_idx = -1;
        for (int neighbor_idx : adj[edge.vertex1_idx]) {
            if (neighbor_idx != edge.vertex2_idx) {
                v0_idx = neighbor_idx;
                break;
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
        for (int neighbor_idx : adj[edge.vertex2_idx]) {
            if (neighbor_idx != edge.vertex1_idx) {
                v3_idx = neighbor_idx;
                break;
            }
        }
        if (v3_idx != -1) {
            v3 = vertices[v3_idx];
        } else {
            v3 = v2;
            v3.position = v2.position + (v2.position - v1.position);
        }

        render_edge(v0, v1, v2, v3, offset);
    }

    glDisable(GL_COLOR_MATERIAL);
}

}
