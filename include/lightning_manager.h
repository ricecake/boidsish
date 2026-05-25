#ifndef LIGHTNING_MANAGER_H
#define LIGHTNING_MANAGER_H

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "IManager.h"
#include "shader.h"

namespace Boidsish {

	class ServiceLocator;
	class LightManager;

	enum class LightningType {
		BOLT,            // Sky to ground
		FORK,            // Branching sky to ground
		CLOUD_TO_CLOUD   // Internal cloud flashes
	};

	struct LightningBoltSegment {
		glm::vec3 start;
		glm::vec3 end;
	};

	struct LightningStrike {
		int id;
		LightningType type;
		std::vector<LightningBoltSegment> segments;
		float lifetime;        // Current lifetime
		float max_lifetime;    // Total duration of the flash
		float intensity;       // Current brightness [0-1]
		glm::vec3 color;
		bool has_spawned_flash;
	};

	class LightningManager : public IManager {
	public:
		LightningManager(ServiceLocator& loc);
		~LightningManager() override;

		void Initialize() override;
		void Update(float deltaTime, float totalTime);
		void Render(const glm::mat4& view, const glm::mat4& projection);

		void TriggerStrike(LightningType type, const glm::vec3& startPos, const glm::vec3& endPos, const glm::vec3& color);

		const std::vector<LightningStrike>& GetActiveStrikes() const { return _activeStrikes; }

		float GetGlobalPulse() const { return _globalPulse; }
		glm::vec3 GetGlobalColor() const { return _globalColor; }

	private:
		struct LightningVertex {
			glm::vec3 pos;
			glm::vec3 color;
			float intensity;
		};

		void GenerateBolt(LightningStrike& strike, const glm::vec3& start, const glm::vec3& end, int depth);

		ServiceLocator& _loc;
		std::vector<LightningStrike> _activeStrikes;
		int _nextStrikeId = 0;

		float _globalPulse = 0.0f;
		glm::vec3 _globalColor = glm::vec3(0.0f);

		unsigned int _vao = 0;
		unsigned int _vbo = 0;
		std::unique_ptr<Shader> _shader;
	};

} // namespace Boidsish

#endif // LIGHTNING_MANAGER_H
