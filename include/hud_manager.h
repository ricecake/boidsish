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

		// Icon management
		void                        AddIcon(const HudIcon& icon);
		void                        UpdateIcon(int id, const HudIcon& icon);
		void                        RemoveIcon(int id);
		const std::vector<HudIcon>& GetIcons() const;
		unsigned int                GetTextureId(const std::string& path);

		// Number management
		void                          AddNumber(const HudNumber& number);
		void                          UpdateNumber(int id, const HudNumber& number);
		void                          RemoveNumber(int id);
		const std::vector<HudNumber>& GetNumbers() const;

		// Gauge management
		void                         AddGauge(const HudGauge& gauge);
		void                         UpdateGauge(int id, const HudGauge& gauge);
		void                         RemoveGauge(int id);
		const std::vector<HudGauge>& GetGauges() const;

	private:
		std::vector<HudIcon>   m_icons;
		std::vector<HudNumber> m_numbers;
		std::vector<HudGauge>  m_gauges;

		std::map<std::string, unsigned int> m_texture_cache;

		unsigned int LoadTexture(const std::string& path);
	};

} // namespace Boidsish
