#ifndef COMPETITIVE_CA_WIDGET_H
#define COMPETITIVE_CA_WIDGET_H

#include "IWidget.h"
#include "CompetitiveCA.h"

namespace Boidsish {
	class Visualizer;

	namespace UI {
		class CompetitiveCAWidget: public IWidget {
		public:
			CompetitiveCAWidget(Visualizer& visualizer);
			~CompetitiveCAWidget() override = default;

			void Draw() override;

			bool IsVisible() const override { return m_show; }
			void SetVisible(bool visible) override { m_show = visible; }

		private:
			Visualizer&   m_visualizer;
			CompetitiveCA m_ca;
			bool          m_show = false;

			// Parameters
			float m_dt = 0.5f;
			int   m_mode = 0; // 0 = Continuous Growth, 1 = Continuous Cyclic, 2 = Soft Voting
			float m_growth = 1.2f;
			float m_competition = 1.0f;
			float m_diffusion = 0.3f;
			float m_sharpness = 0.2f;
			float m_fuzziness = 0.15f;
			int   m_radius = 2;
			int   m_species_count = 4;
			int   m_seed_pattern = 0;
			bool  m_paused = false;
			float m_elapsed_time = 0.0f;

			bool  m_initialized = false;
		};
	} // namespace UI
} // namespace Boidsish

#endif // COMPETITIVE_CA_WIDGET_H
