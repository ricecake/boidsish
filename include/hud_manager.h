#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "hud.h"

namespace Boidsish {

	class HudManager {
	public:
		HudManager();
		~HudManager();

		// Modern API
		std::shared_ptr<HudIcon> AddIcon(
			const std::string& path,
			HudAlignment       alignment = HudAlignment::TOP_LEFT,
			glm::vec2          position = {0, 0},
			glm::vec2          size = {64, 64}
		);
		std::shared_ptr<HudNumber> AddNumber(
			float              value = 0.0f,
			const std::string& label = "",
			HudAlignment       alignment = HudAlignment::TOP_RIGHT,
			glm::vec2          position = {-10, 10},
			int                precision = 2
		);
		std::shared_ptr<HudGauge> AddGauge(
			float              value = 0.0f,
			const std::string& label = "",
			HudAlignment       alignment = HudAlignment::BOTTOM_CENTER,
			glm::vec2          position = {0, -50},
			glm::vec2          size = {200, 20}
		);

		void AddElement(std::shared_ptr<HudElement> element);
		void RemoveElement(std::shared_ptr<HudElement> element);
		void Update(float dt, const Camera& camera);

		const std::vector<std::shared_ptr<HudElement>>& GetElements() const { return m_elements; }

		// Legacy API compatibility (deprecated but functional)
		void AddIcon(const HudIcon& icon);
		void UpdateIcon(int id, const HudIcon& icon);
		void RemoveIcon(int id);

		void AddNumber(const HudNumber& number);
		void UpdateNumber(int id, const HudNumber& number);
		void RemoveNumber(int id);

		void AddGauge(const HudGauge& gauge);
		void UpdateGauge(int id, const HudGauge& gauge);
		void RemoveGauge(int id);

		// Helpers for rendering
		unsigned int     GetTextureId(const std::string& path);
		static glm::vec2 GetAlignmentPosition(HudAlignment alignment, glm::vec2 elementSize, glm::vec2 offset);

	private:
		std::vector<std::shared_ptr<HudElement>> m_elements;

		std::map<std::string, unsigned int> m_texture_cache;

		unsigned int LoadTexture(const std::string& path);
	};

} // namespace Boidsish
