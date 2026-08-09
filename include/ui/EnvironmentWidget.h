#ifndef ENVIRONMENT_WIDGET_H
#define ENVIRONMENT_WIDGET_H

#include "IWidget.h"

namespace Boidsish {
	class Visualizer;

	namespace UI {
		class EnvironmentWidget: public IWidget {
		public:
			EnvironmentWidget(Visualizer& visualizer);
			void Draw() override;

		private:
			Visualizer& m_visualizer;
			bool        m_show = true;

			// Injection parameters
			float m_targetPressure = 1013.25f;
			float m_targetTemperature = 288.15f;
			float m_targetAerosol = 0.01f;
			float m_burstStrength = 0.05f;

			// LTC Area Lights configuration state
			float m_ltc_pos[3] = { 0.0f, 6.0f, 0.0f };
			float m_ltc_color[3] = { 1.0f, 0.0f, 0.5f };
			float m_ltc_intensity = 15.0f;
			float m_ltc_size = 6.0f;
			float m_ltc_height = 1.0f;
			int m_ltc_circle_segments = 8;

			float m_ltc_p0[3] = { -4.0f, 0.0f, -4.0f };
			float m_ltc_p1[3] = { -2.0f, 2.0f, 0.0f };
			float m_ltc_p2[3] = { 2.0f, -2.0f, 0.0f };
			float m_ltc_p3[3] = { 4.0f, 0.0f, 4.0f };
		};
	} // namespace UI
} // namespace Boidsish

#endif // ENVIRONMENT_WIDGET_H
