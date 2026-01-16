#ifndef BOIDSISH_EFFECTSWIDGET_H
#define BOIDSISH_EFFECTSWIDGET_H

#include "IWidget.h"

namespace Boidsish {

	namespace UI {

		class EffectsWidget: public IWidget {
		public:
			EffectsWidget();
			void Draw() override;

		private:
			bool m_show = true;
		};

	} // namespace UI
} // namespace Boidsish

#endif // BOIDSISH_EFFECTSWIDGET_H
