#ifndef RENDER_WIDGET_H
#define RENDER_WIDGET_H

#include "IWidget.h"

namespace Boidsish {
	class Visualizer;

	namespace UI {
		class RenderWidget: public IWidget {
		public:
			RenderWidget(Visualizer& visualizer);
			void Draw() override;

			bool IsVisible() const override { return m_show; }
			void SetVisible(bool visible) override { m_show = visible; }

		private:
			Visualizer& m_visualizer;
			bool        m_show = true;
		};
	} // namespace UI
} // namespace Boidsish

#endif // RENDER_WIDGET_H
