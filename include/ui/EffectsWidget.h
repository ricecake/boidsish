#ifndef BOIDSISH_EFFECTSWIDGET_H
#define BOIDSISH_EFFECTSWIDGET_H

#include "IWidget.h"

namespace Boidsish {

	class Visualizer; // Forward declaration

	namespace UI {

		class EffectsWidget: public IWidget {
		public:
			EffectsWidget(Visualizer& visualizer);
			void Draw() override;

		private:
			Visualizer& m_visualizer;
			bool        m_show = true;
		};

	} // namespace UI
} // namespace Boidsish

#endif // BOIDSISH_EFFECTSWIDGET_H
