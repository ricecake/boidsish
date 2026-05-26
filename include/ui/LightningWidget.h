#ifndef LIGHTNING_WIDGET_H
#define LIGHTNING_WIDGET_H

#include "IWidget.h"
#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace Boidsish {
	class Visualizer;

	namespace UI {
		class LightningWidget : public IWidget {
		public:
			LightningWidget(Visualizer& visualizer);
			void Draw() override;

		private:
			struct DelayedStrike {
				float timeLeft;
				int type; // 0: BOLT, 1: FORK, 2: CLOUD_TO_CLOUD
				glm::vec3 color;
			};

			void TriggerManualStrike(int type, const glm::vec3& color);
			glm::vec3 GetRandomPositionNearCamera();

			Visualizer& m_visualizer;
			bool m_show = true;
			bool m_manualAutoTrigger = false;
			float m_strikeDelay = 1.0f;
			glm::vec3 m_manualColor = glm::vec3(1.0f, 1.0f, 1.0f);
			std::vector<DelayedStrike> m_delayedStrikes;
		};
	} // namespace UI
} // namespace Boidsish

#endif // LIGHTNING_WIDGET_H
