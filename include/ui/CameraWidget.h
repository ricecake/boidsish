#ifndef CAMERA_WIDGET_H
#define CAMERA_WIDGET_H
#include "IWidget.h"
#include <optional>
#include <glm/glm.hpp>
namespace Boidsish {
	class Visualizer;
	namespace UI {
		class CameraWidget: public IWidget {
		public:
			CameraWidget(Visualizer& visualizer);
			void Draw() override;
		private:
			Visualizer& m_visualizer;
			bool m_show = true;
			bool m_is_picking_enabled = false;
			std::optional<glm::vec3> m_last_picked_pos;
		};
	}
}
#endif
