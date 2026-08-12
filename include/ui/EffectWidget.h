#ifndef EFFECT_WIDGET_H
#define EFFECT_WIDGET_H

#include "IWidget.h"

namespace Boidsish {
	class Visualizer;

	namespace UI {
		class EffectWidget: public IWidget {
		public:
			EffectWidget(Visualizer& visualizer);
			void Draw() override;

			bool IsVisible() const override { return m_show; }
			void SetVisible(bool visible) override { m_show = visible; }

		private:
			Visualizer& m_visualizer;
			bool        m_show = true;
		};
	} // namespace UI
} // namespace Boidsish

#endif // EFFECT_WIDGET_H
