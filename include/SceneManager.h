#ifndef SCENE_MANAGER_H
#define SCENE_MANAGER_H

#include "Scene.h"
#include "Config.h"
#include <string>
#include <vector>

namespace Boidsish {

	class SceneManager {
	public:
		SceneManager(const std::string& sceneFolder);

		std::vector<std::string> GetDictionaries();
		void                     LoadDictionary(const std::string& name);
		void                     SaveDictionary(const std::string& name);

		const std::vector<Scene>& GetScenes() const;
		void                      AddScene(const Scene& scene);
		void                      RemoveScene(int index);

		std::string GetCurrentDictionaryName() const { return m_currentDictionary; }

	private:
		std::string        m_sceneFolder;
		std::vector<Scene> m_scenes;
		std::string        m_currentDictionary;

		void  SerializeScene(Config& config, const Scene& scene, int index);
		Scene DeserializeScene(const Config& config, int index);
	};

} // namespace Boidsish

#endif // SCENE_MANAGER_H
