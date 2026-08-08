#ifndef SYSTEM_WIDGET_H
#define SYSTEM_WIDGET_H

#include <optional>

#include "IWidget.h"
#include <glm/glm.hpp>

namespace Boidsish {
	class Visualizer;
	class SceneManager;

	namespace UI {
		class SystemWidget: public IWidget {
		public:
			SystemWidget(Visualizer& visualizer, SceneManager& sceneManager);
			void Draw() override;

		private:
			Visualizer&   m_visualizer;
			SceneManager& m_sceneManager;
			bool          m_show = true;

			// Picking state (migrated from EffectsWidget)
			bool                     m_is_picking_enabled = false;
			std::optional<glm::vec3> m_last_picked_pos;

			// Cloud Painting state
			bool  m_is_painting_enabled = false;
			int   m_paint_mode = 0; // 0 = Add, 1 = Erase
			float m_brush_radius = 2000.0f;
			float m_brush_strength = 0.5f;
			float m_delete_lifetime = 30.0f;
			float m_decay_rate = 0.1f;

			// Scene management state (migrated from SceneWidget)
			char m_saveName[128];
			bool m_saveCamera;
			bool m_moveCamera;
			bool m_saveEffects;
			bool m_applyEffects;
			char m_newDictName[128];
		};
	} // namespace UI
} // namespace Boidsish

#endif // SYSTEM_WIDGET_H
