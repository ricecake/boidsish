#ifndef LIGHTING_WIDGET_H
#define LIGHTING_WIDGET_H

#include "IWidget.h"
#include "light_manager.h"

namespace Boidsish {
	namespace UI {

		class LightingWidget: public IWidget {
		public:
			LightingWidget(LightManager& lightManager);
			void Draw() override;

		private:
			LightManager& _lightManager;
			bool          m_show;
		};

	} // namespace UI
} // namespace Boidsish

#endif // LIGHTING_WIDGET_H
