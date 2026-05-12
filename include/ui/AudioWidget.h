#pragma once

#include "IWidget.h"

namespace Boidsish {

	class Visualizer;

	namespace UI {

		class AudioWidget : public IWidget {
		public:
			AudioWidget(Visualizer& visualizer);
			virtual ~AudioWidget() = default;

			void Draw() override;

			void SetVisible(bool visible) override { m_visible = visible; }
			bool IsVisible() const override { return m_visible; }

		private:
			Visualizer& m_visualizer;
			bool        m_visible = true;
		};

	} // namespace UI

} // namespace Boidsish
