#include "ReactionDiffusionManager.h"
#include "logger.h"
#include <shader.h>
#include <vector>
#include <random>

namespace Boidsish {
    ReactionDiffusionManager::ReactionDiffusionManager(int width, int height)
        : m_width(width), m_height(height), m_current_texture_index(0) {
    }

    ReactionDiffusionManager::~ReactionDiffusionManager() {
        glDeleteFramebuffers(2, m_fbos);
        glDeleteTextures(2, m_textures);
        glDeleteVertexArrays(1, &m_vao);
    }

    void ReactionDiffusionManager::Initialize() {
        m_shader = std::make_unique<Shader>("shaders/passthrough.vert", "shaders/reaction_diffusion.frag");
        InitializeFBOs();

        // Create a dummy VAO for drawing a fullscreen triangle
        glGenVertexArrays(1, &m_vao);

        Reset();
    }

    void ReactionDiffusionManager::InitializeFBOs() {
        glGenFramebuffers(2, m_fbos);
        glGenTextures(2, m_textures);

        for (unsigned int i = 0; i < 2; i++) {
            glBindFramebuffer(GL_FRAMEBUFFER, m_fbos[i]);
            glBindTexture(GL_TEXTURE_2D, m_textures[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, m_width, m_height, 0, GL_RG, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_textures[i], 0);

            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                logger::ERROR("ReactionDiffusion FBO not complete!");
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void ReactionDiffusionManager::Reset() {
        // Initial state: A grid of (1,0) with a central square of (0,1)
        std::vector<float> initial_data(m_width * m_height * 2);
        for (int y = 0; y < m_height; ++y) {
            for (int x = 0; x < m_width; ++x) {
                int index = (y * m_width + x) * 2;
                initial_data[index] = 1.0f; // A
                initial_data[index + 1] = 0.0f; // B
            }
        }

        // Add a seed area
        int seed_size = 20;
        int center_x = m_width / 2;
        int center_y = m_height / 2;
        for (int y = center_y - seed_size / 2; y < center_y + seed_size / 2; ++y) {
            for (int x = center_x - seed_size / 2; x < center_x + seed_size / 2; ++x) {
                if (x > 0 && x < m_width && y > 0 && y < m_height) {
                    int index = (y * m_width + x) * 2;
                    initial_data[index + 1] = 1.0f; // B
                }
            }
        }

        glBindTexture(GL_TEXTURE_2D, m_textures[0]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_RG, GL_FLOAT, initial_data.data());
        glBindTexture(GL_TEXTURE_2D, m_textures[1]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_RG, GL_FLOAT, initial_data.data());
    }


    void ReactionDiffusionManager::Update() {
        if (m_paused) return;

        glDisable(GL_DEPTH_TEST);
        glViewport(0, 0, m_width, m_height);

        m_shader->use();
        m_shader->setFloat("u_feed_rate", m_feed_rate);
        m_shader->setFloat("u_kill_rate", m_kill_rate);
        m_shader->setFloat("u_diffuse_a", m_diffuse_rate_a);
        m_shader->setFloat("u_diffuse_b", m_diffuse_rate_b);
        m_shader->setFloat("u_timestep", m_timestep);
        m_shader->setVec2("u_resolution", (float)m_width, (float)m_height);

        glBindVertexArray(m_vao);

        for (int i = 0; i < m_iterations; ++i) {
            glBindFramebuffer(GL_FRAMEBUFFER, m_fbos[1 - m_current_texture_index]);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_textures[m_current_texture_index]);
            m_shader->setInt("u_texture", 0);

            glDrawArrays(GL_TRIANGLES, 0, 3);

            SwapBuffers();
        }

        glBindVertexArray(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glEnable(GL_DEPTH_TEST);
    }

    void ReactionDiffusionManager::SwapBuffers() {
        m_current_texture_index = 1 - m_current_texture_index;
    }

}
