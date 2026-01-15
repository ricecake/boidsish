#pragma once

#include "IWidget.h"

namespace Boidsish {

    class Visualizer;

	namespace UI {
		class ConfigWidget: public IWidget {
		public:
			ConfigWidget(Visualizer& visualizer);
			void Draw() override;

		private:
			bool m_show = true;
            Visualizer& m_visualizer;
		};
	} // namespace UI
} // namespace Boidsish
