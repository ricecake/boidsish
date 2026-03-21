#include "voxel_volume.h"
#include "render_context.h"
#include "shader_table.h"
#include "shader.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {

std::shared_ptr<Shader> VoxelVolume::voxel_shader = nullptr;
ShaderHandle VoxelVolume::voxel_shader_handle = ShaderHandle(0);

static unsigned int v_box_vao = 0;
static unsigned int v_box_vbo = 0;
static unsigned int v_box_ebo = 0;
static int          v_box_index_count = 0;

VoxelVolume::VoxelVolume(float x, float y, float z, float size) :
    Shape(0, x, y, z), size_(size) {
    SetScale(glm::vec3(size));
}

void VoxelVolume::InitializeShaders() {
    if (!voxel_shader) {
        voxel_shader = std::make_shared<Shader>("shaders/effects/voxel_viz.vert", "shaders/effects/voxel_viz.frag");
    }
}

void VoxelVolume::render() const {
    // Legacy path - not used in MDI
}

void VoxelVolume::render(Shader& shader, const glm::mat4& model_matrix) const {
    // Legacy path
}

glm::mat4 VoxelVolume::GetModelMatrix() const {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), GetPosition());
    model = model * glm::mat4_cast(GetRotation());
    model = glm::scale(model, GetScale());
    return model;
}

void VoxelVolume::GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const {
    if (v_box_vao == 0) {
        // Initialize simple unit box mesh
        float vertices[] = {
            -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,
            -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f
        };
        unsigned int indices[] = {
            0, 1, 2, 2, 3, 0,  4, 5, 6, 6, 7, 4,
            0, 4, 7, 7, 3, 0,  1, 5, 6, 6, 2, 1,
            3, 2, 6, 6, 7, 3,  0, 1, 5, 5, 4, 0
        };
        v_box_index_count = 36;

        glGenVertexArrays(1, &v_box_vao);
        glBindVertexArray(v_box_vao);
        glGenBuffers(1, &v_box_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, v_box_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glGenBuffers(1, &v_box_ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, v_box_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    RenderPacket packet;
    packet.vao = v_box_vao;
    packet.index_count = v_box_index_count;
    packet.draw_mode = GL_TRIANGLES;
    packet.index_type = GL_UNSIGNED_INT;
    packet.shader_id = voxel_shader ? voxel_shader->ID : 0;
    packet.shader_handle = voxel_shader_handle;

    packet.uniforms.model = GetModelMatrix();
    packet.uniforms.color = glm::vec4(GetR(), GetG(), GetB(), GetA());

    packet.sort_key = CalculateSortKey(
        RenderLayer::Transparent,
        packet.shader_handle,
        packet.vao,
        packet.draw_mode,
        true,
        MaterialHandle(0),
        context.CalculateNormalizedDepth(GetPosition())
    );

    out_packets.push_back(packet);
}

} // namespace Boidsish
