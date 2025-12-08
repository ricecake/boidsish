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

void render_frustum_segment(const Vector3& p1, const Vector3& p2, float r1, float g1, float b1, float a1, float r2, float g2, float b2, float a2, float radius1, float radius2) {
    Vector3 axis = p2 - p1;
    float height = axis.Magnitude();

    if (height < 1e-6) return;

    axis.Normalize();

    glPushMatrix();
    glTranslatef(p1.x, p1.y, p1.z);

    Vector3 z_axis(0, 0, 1);
    float dot = z_axis.Dot(axis);

    if (std::abs(dot) > 0.99999f) {
        if (dot < 0) {
            glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
        }
    } else {
        Vector3 rot_axis = z_axis.Cross(axis);
        float rot_angle = acos(dot) * 180.0f / M_PI;
        glRotatef(rot_angle, rot_axis.x, rot_axis.y, rot_axis.z);
    }

    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= CYLINDER_SEGMENTS; ++i) {
        float angle = 2.0f * M_PI * (float)i / CYLINDER_SEGMENTS;

        Vector3 normal(cos(angle), sin(angle), 0); // Approximate normal
        glNormal3f(normal.x, normal.y, normal.z);

        // Vertex 1 (bottom circle)
        float x1 = cos(angle) * radius1;
        float y1 = sin(angle) * radius1;
        glColor4f(r1, g1, b1, a1);
        glVertex3f(x1, y1, 0);

        // Vertex 2 (top circle)
        float x2 = cos(angle) * radius2;
        float y2 = sin(angle) * radius2;
        glColor4f(r2, g2, b2, a2);
        glVertex3f(x2, y2, height);
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

    Vector3 prev_point = p1;

    for (int i = 1; i <= CURVE_SEGMENTS; ++i) {
        float t = (float)i / CURVE_SEGMENTS;
        float u = 1.0f - t;

        Vector3 current_point = CatmullRom(t, p0, p1, p2, p3);

        float r = u * v1.r + t * v2.r;
        float g = u * v1.g + t * v2.g;
        float b = u * v1.b + t * v2.b;
        float a = u * v1.a + t * v2.a;

        float prev_t = (float)(i-1)/CURVE_SEGMENTS;
        float prev_u = 1.0f - prev_t;
        float prev_r = prev_u * v1.r + prev_t * v2.r;
        float prev_g = prev_u * v1.g + prev_t * v2.g;
        float prev_b = prev_u * v1.b + prev_t * v2.b;
        float prev_a = prev_u * v1.a + prev_t * v2.a;

        float current_radius = (u * v1.size + t * v2.size) * EDGE_RADIUS_SCALE;
        float prev_radius = (prev_u * v1.size + prev_t * v2.size) * EDGE_RADIUS_SCALE;

        render_frustum_segment(prev_point, current_point, prev_r, prev_g, prev_b, prev_a, r, g, b, a, prev_radius, current_radius);
        prev_point = current_point;
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
