#pragma once

#include "IWidget.h"
#include "post_processing/PostProcessingManager.h"

namespace Boidsish {
	namespace UI {

		class PostProcessingWidget: public IWidget {
		public:
			PostProcessingWidget(PostProcessing::PostProcessingManager& manager);
			void Draw() override;

		private:
			PostProcessing::PostProcessingManager& manager_;
		};

	} // namespace UI
} // namespace Boidsish
