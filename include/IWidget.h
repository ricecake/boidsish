#pragma once

namespace Boidsish {
	namespace UI {
		class IWidget {
		public:
			virtual ~IWidget() = default;
			virtual void Draw() = 0;
		};
	} // namespace UI
} // namespace Boidsish
