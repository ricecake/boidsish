#pragma once

#include "IWidget.h"

namespace Boidsish {
	class Visualizer;

	namespace UI {
		class EffectWidget: public IWidget {
		public:
			EffectWidget(Visualizer& visualizer);
			void Draw() override;

		private:
			Visualizer& m_visualizer;
			bool        m_show = true;
		};
	} // namespace UI
} // namespace Boidsish
