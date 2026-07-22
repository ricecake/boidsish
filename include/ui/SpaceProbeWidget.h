#pragma once

#include "IWidget.h"

namespace Boidsish {
	class Visualizer;

	namespace UI {

		class SpaceProbeWidget : public IWidget {
		public:
			SpaceProbeWidget(Visualizer& visualizer);
			virtual ~SpaceProbeWidget() = default;

			void Draw() override;

			bool IsVisible() const override { return m_show; }
			void SetVisible(bool visible) override { m_show = visible; }

		private:
			Visualizer& m_visualizer;
			bool        m_show = true;
		};

	} // namespace UI
} // namespace Boidsish
