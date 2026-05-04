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
		};
	} // namespace UI
} // namespace Boidsish

#endif // ENVIRONMENT_WIDGET_H
