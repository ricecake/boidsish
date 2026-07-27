#ifndef LIGHTING_WIDGET_H
#define LIGHTING_WIDGET_H

#include "IWidget.h"
#include "ui/BloomSettingsComponent.h"
#include "ui/AmbientSettingsComponent.h"
#include "ui/LightsComponent.h"

namespace Boidsish {
	class Visualizer;

	namespace UI {
		class LightingWidget: public IWidget {
		public:
			LightingWidget(Visualizer& visualizer);
			void Draw() override;

		private:
			Visualizer& m_visualizer;
			bool        m_show = true;

			BloomSettingsComponent   m_bloomSettings;
			AmbientSettingsComponent m_ambientSettings;
			LightsComponent          m_lights;
		};
	} // namespace UI
} // namespace Boidsish

#endif // LIGHTING_WIDGET_H
