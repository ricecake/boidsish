#pragma once

namespace Boidsish {
	namespace UI {
		class IWidget {
		public:
			virtual ~IWidget() = default;
			virtual void Draw() = 0;

			virtual bool IsHud() const { return false; }
		};
	} // namespace UI
} // namespace Boidsish
