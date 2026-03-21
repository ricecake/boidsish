#pragma once

namespace Boidsish {
	namespace UI {
		class IWidget {
		public:
			virtual ~IWidget() = default;
			virtual void Draw() = 0;

			virtual bool IsHud() const { return false; }

			virtual void SetVisible(bool) {}

			virtual bool IsVisible() const { return true; }
		};
	} // namespace UI
} // namespace Boidsish
