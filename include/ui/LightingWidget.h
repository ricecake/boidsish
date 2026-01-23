#ifndef BOIDSISH_LIGHTINGWIDGET_H
#define BOIDSISH_LIGHTINGWIDGET_H

#include "IWidget.h"

namespace Boidsish {
    class Visualizer;

	namespace UI {

		class LightingWidget: public IWidget {
		public:
			LightingWidget(Visualizer& visualizer);
			void Draw() override;

		private:
            Visualizer& m_visualizer;
			bool m_show = true;
		};

	} // namespace UI
} // namespace Boidsish

#endif // BOIDSISH_LIGHTINGWIDGET_H
