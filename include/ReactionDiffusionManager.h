#pragma once

#include <memory>
#include <GL/glew.h>

class Shader;

namespace Boidsish {
    class ReactionDiffusionManager {
    public:
        ReactionDiffusionManager(int width, int height);
        ~ReactionDiffusionManager();

        void Initialize();
        void Update();
        void Reset();
        void Resize(int width, int height);

        GLuint GetTexture() const { return m_textures[m_current_texture_index]; }

        // Simulation parameters
        float& GetFeedRate() { return m_feed_rate; }
        float& GetKillRate() { return m_kill_rate; }
        float& GetDiffuseRateA() { return m_diffuse_rate_a; }
        float& GetDiffuseRateB() { return m_diffuse_rate_b; }
        float& GetTimestep() { return m_timestep; }
        int& GetIterations() { return m_iterations; }
        bool& IsPaused() { return m_paused; }

    private:
        void InitializeFBOs();
        void SwapBuffers();

        int m_width, m_height;
        std::unique_ptr<Shader> m_shader;

        GLuint m_fbos[2];
        GLuint m_textures[2];
        int m_current_texture_index;

        GLuint m_vao;

        // Parameters
        float m_feed_rate = 0.055f;
        float m_kill_rate = 0.062f;
        float m_diffuse_rate_a = 1.0f;
        float m_diffuse_rate_b = 0.5f;
        float m_timestep = 1.0f;
        int m_iterations = 1;
        bool m_paused = false;
    };
}
