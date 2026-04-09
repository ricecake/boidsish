#pragma once

#include "IWidget.h"
#include <string>
#include <vector>

namespace Boidsish {
	class ScriptManager;

	namespace UI {
		class ScriptWidget: public IWidget {
		public:
			ScriptWidget(ScriptManager& scriptManager);
			virtual void Draw() override;

			virtual void SetVisible(bool visible) override { m_show = visible; }
			virtual bool IsVisible() const override { return m_show; }

		private:
			ScriptManager& m_scriptManager;
			bool           m_show = false;
			char           m_inputBuffer[1024];
			std::vector<std::pair<std::string, std::string>> m_history;
			bool m_scrollToBottom = false;
		};
	} // namespace UI
} // namespace Boidsish
