#ifndef SPECIAL_EFFECTS_WIDGET_H
#define SPECIAL_EFFECTS_WIDGET_H
#include "IWidget.h"
namespace Boidsish {
	class Visualizer;
	namespace UI {
		class SpecialEffectsWidget: public IWidget {
		public:
			SpecialEffectsWidget(Visualizer* visualizer);
			void Draw() override;
		private:
			Visualizer* m_visualizer;
			bool m_show = true;
		};
	}
}
#endif
