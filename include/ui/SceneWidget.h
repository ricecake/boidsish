#ifndef SCENE_WIDGET_H
#define SCENE_WIDGET_H

#include "IWidget.h"
#include "SceneManager.h"
#include "graphics.h"

namespace Boidsish {
	namespace UI {

		class SceneWidget: public IWidget {
		public:
			SceneWidget(SceneManager& sceneManager, Visualizer& visualizer);
			void Draw() override;

		private:
			SceneManager& _sceneManager;
			Visualizer&   _visualizer;
			bool          m_show;

			char m_saveName[128];
			bool m_saveCamera;
			bool m_moveCamera;
			bool m_saveEffects;
			bool m_applyEffects;

			char m_newDictName[128];
		};

	} // namespace UI
} // namespace Boidsish

#endif // SCENE_WIDGET_H
