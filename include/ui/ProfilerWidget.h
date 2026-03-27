#pragma once

#include <string>

#include "IWidget.h"

namespace Boidsish {
	namespace UI {
		class ProfilerWidget: public IWidget {
		public:
			ProfilerWidget();
			void Draw() override;

			bool IsHud() const override { return false; }

			void SetVisible(bool visible) override;
			bool IsVisible() const override;

		private:
			bool m_show = false;
			bool m_treeView = true;
		};
	} // namespace UI
} // namespace Boidsish
