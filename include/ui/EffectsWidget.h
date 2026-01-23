#ifndef BOIDSISH_EFFECTSWIDGET_H
#define BOIDSISH_EFFECTSWIDGET_H

#include "IWidget.h"
#include <glm/glm.hpp>
#include <optional>

namespace Boidsish {
	class Visualizer;
	namespace UI {

		class EffectsWidget : public IWidget {
		public:
			EffectsWidget(Visualizer* visualizer);
			void Draw() override;

		private:
			bool                     m_show = true;
			Visualizer*              m_visualizer;
			bool                     m_is_picking_enabled = false;
			std::optional<glm::vec3> m_last_picked_pos;
		};

	} // namespace UI
} // namespace Boidsish

#endif // BOIDSISH_EFFECTSWIDGET_H
