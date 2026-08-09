#pragma once

#include "IWidget.h"
#include "entity.h"
#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>

namespace Boidsish {
	class Visualizer;

	namespace UI {
		class SpawnablesWidget : public IWidget {
		public:
			SpawnablesWidget(Visualizer& visualizer);
			~SpawnablesWidget() override;

			void Draw() override;
			bool IsVisible() const override { return m_show; }
			void SetVisible(bool visible) override { m_show = visible; }

		private:
			Visualizer& m_visualizer;
			bool m_show = true;

			// Active spawned entities under our control
			std::unique_ptr<EntityHandler> m_entity_handler;

			struct BalloonSpawnSettings {
				char name[64] = "Hot Air Balloon";
				float x = 0.0f;
				float y = 50.0f;
				float z = 0.0f;
				float size = 6.0f;
				float color[4] = { 0.9f, 0.4f, 0.2f, 1.0f };
				float buoyancy = 12.0f;
				float wind_response = 1.0f;
				float stiffness = 30.0f;
				float damping = 2.0f;
			};

			BalloonSpawnSettings m_settings;
			bool m_first_draw = true;

			void SpawnBalloon();
		};
	} // namespace UI
} // namespace Boidsish
