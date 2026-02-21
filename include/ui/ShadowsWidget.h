#pragma once

#include "IWidget.h"
#include <glm/glm.hpp>

namespace Boidsish {
	class Visualizer;

	namespace UI {

		class ShadowsWidget : public IWidget {
		public:
			ShadowsWidget(Visualizer& visualizer);
			void Draw() override;

		private:
			Visualizer& _visualizer;
			bool _show = true;
			float _sdfIntensity = 1.0f;
			float _sdfSoftness = 10.0f;
			float _sdfMaxDist = 2.0f;
			float _sdfBias = 0.05f;
			bool _sdfDebug = false;
		};

	} // namespace UI
} // namespace Boidsish
