#pragma once

#include "IWidget.h"
#include <GL/glew.h>
#include <memory>
#include <glm/glm.hpp>
#include "shader.h"

namespace Boidsish {
    class Visualizer;

    namespace UI {

        class MapWidget : public IWidget {
        public:
            MapWidget(Visualizer& visualizer);
            ~MapWidget();

            void Draw() override;
            bool IsHud() const override { return false; }
            bool IsVisible() const override { return m_show; }
            void SetVisible(bool visible) override { m_show = visible; }

        private:
            void InitializeResources();
            void RenderMap(int renderWidth = 512, int renderHeight = 512);

            Visualizer& m_visualizer;
            bool m_show = false;

            GLuint m_fbo = 0;
            GLuint m_mapTexture = 0;
            int m_textureWidth = 512;
            int m_textureHeight = 512;

            std::unique_ptr<Shader> m_mapShader;
            GLuint m_quadVao = 0;
            GLuint m_quadVbo = 0;

            // Display settings
            bool m_showWind = true;
            bool m_showTemperature = false;
            bool m_showHumidity = false;
            bool m_showPressure = false;
            bool m_showAerosol = false;
            bool m_showCloudCover = false;
            bool m_showCloudHeight = false;
            bool m_showCloudShadow = false;

            float m_mapViewScale = 1.0f;
            glm::vec2 m_mapOffset = glm::vec2(0.0f);

            int m_selectedLayer = 0; // 0: Auto/Combined, 1: Cloud Weather, 2: LBM Weather, 3: Cloud Shadow

            // Drawing tools state
            bool m_enableDrawing = false;
            int m_drawOp = 1; // 0: Erase, 1: Steep Overwrite, 2: Smooth Min, 3: Smooth Max
            bool m_channelMask[4] = { true, true, true, true }; // R (Coverage), G (Height), B (Thickness), A (Density)
            float m_brushTargetValue[4] = { 0.8f, 0.5f, 0.5f, 0.5f };
            float m_brushRadiusWorld = 2000.0f;
            float m_brushStrength = 0.5f;
            float m_smoothness = 0.1f;
        };

    } // namespace UI
} // namespace Boidsish
