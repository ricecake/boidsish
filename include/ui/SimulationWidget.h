#ifndef SIMULATION_WIDGET_H
#define SIMULATION_WIDGET_H
#include "IWidget.h"
#include <string>
namespace Boidsish {
	class Visualizer;
	namespace UI {
		class SimulationWidget: public IWidget {
		public:
			SimulationWidget(Visualizer& visualizer);
			void Draw() override;
		private:
			Visualizer& m_visualizer;
			bool m_show = true;
		};
	}
}
#endif
