#pragma once

#include "IWidget.h"

namespace Boidsish {
	namespace UI {
		class ConfigWidget: public IWidget {
		public:
			ConfigWidget();
			void Draw() override;

		private:
			bool m_show = true;
		};
	} // namespace UI
} // namespace Boidsish
