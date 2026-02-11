#include "hud_manager.h"

#include <algorithm>
#include <cstdio>
#include <stdexcept>

#include "asset_manager.h"
#include "graphics.h"
#include "imgui.h"
#include "logger.h"
#include "stb_image.h"
#include <GL/glew.h>

namespace Boidsish {

	HudManager::HudManager() {}

	HudManager::~HudManager() {}

	// Modern API
	std::shared_ptr<HudIcon>
	HudManager::AddIcon(const std::string& path, HudAlignment alignment, glm::vec2 position, glm::vec2 size) {
		auto icon = std::make_shared<HudIcon>(path, alignment, position, size);
		m_elements.push_back(icon);
		return icon;
	}

	std::shared_ptr<HudNumber> HudManager::AddNumber(
		float              value,
		const std::string& label,
		HudAlignment       alignment,
		glm::vec2          position,
		int                precision
	) {
		auto number = std::make_shared<HudNumber>(value, label, alignment, position, precision);
		m_elements.push_back(number);
		return number;
	}

	std::shared_ptr<HudGauge> HudManager::AddGauge(
		float              value,
		const std::string& label,
		HudAlignment       alignment,
		glm::vec2          position,
		glm::vec2          size
	) {
		auto gauge = std::make_shared<HudGauge>(value, label, alignment, position, size);
		m_elements.push_back(gauge);
		return gauge;
	}

	void HudManager::AddElement(std::shared_ptr<HudElement> element) {
		m_elements.push_back(element);
	}

	void HudManager::RemoveElement(std::shared_ptr<HudElement> element) {
		m_elements.erase(std::remove(m_elements.begin(), m_elements.end(), element), m_elements.end());
	}

	void HudManager::Update(float dt, const Camera& camera) {
		for (auto& element : m_elements) {
			element->Update(dt, camera);
		}
	}

	// Legacy management (delegates to modern API)
	void HudManager::AddIcon(const HudIcon& icon) {
		auto newIcon = AddIcon(icon.GetTexturePath(), icon.GetAlignment(), icon.GetPosition(), icon.GetSize());
		newIcon->SetId(icon.GetId());
		newIcon->SetHighlighted(icon.IsHighlighted());
	}

	void HudManager::UpdateIcon(int id, const HudIcon& icon) {
		for (auto& element : m_elements) {
			if (auto hIcon = std::dynamic_pointer_cast<HudIcon>(element)) {
				if (hIcon->GetId() == id) {
					hIcon->SetTexturePath(icon.GetTexturePath());
					hIcon->SetAlignment(icon.GetAlignment());
					hIcon->SetPosition(icon.GetPosition());
					hIcon->SetSize(icon.GetSize());
					hIcon->SetHighlighted(icon.IsHighlighted());
					return;
				}
			}
		}
	}

	void HudManager::RemoveIcon(int id) {
		m_elements.erase(
			std::remove_if(
				m_elements.begin(),
				m_elements.end(),
				[id](const std::shared_ptr<HudElement>& e) { return e->GetId() == id; }
			),
			m_elements.end()
		);
	}

	void HudManager::AddNumber(const HudNumber& number) {
		auto newNumber = AddNumber(
			number.GetValue(),
			number.GetLabel(),
			number.GetAlignment(),
			number.GetPosition(),
			number.GetPrecision()
		);
		newNumber->SetId(number.GetId());
	}

	void HudManager::UpdateNumber(int id, const HudNumber& number) {
		for (auto& element : m_elements) {
			if (auto hNum = std::dynamic_pointer_cast<HudNumber>(element)) {
				if (hNum->GetId() == id) {
					hNum->SetValue(number.GetValue());
					hNum->SetLabel(number.GetLabel());
					hNum->SetAlignment(number.GetAlignment());
					hNum->SetPosition(number.GetPosition());
					hNum->SetPrecision(number.GetPrecision());
					return;
				}
			}
		}
	}

	void HudManager::RemoveNumber(int id) {
		m_elements.erase(
			std::remove_if(
				m_elements.begin(),
				m_elements.end(),
				[id](const std::shared_ptr<HudElement>& e) { return e->GetId() == id; }
			),
			m_elements.end()
		);
	}

	void HudManager::AddGauge(const HudGauge& gauge) {
		auto newGauge =
			AddGauge(gauge.GetValue(), gauge.GetLabel(), gauge.GetAlignment(), gauge.GetPosition(), gauge.GetSize());
		newGauge->SetId(gauge.GetId());
	}

	void HudManager::UpdateGauge(int id, const HudGauge& gauge) {
		for (auto& element : m_elements) {
			if (auto hGauge = std::dynamic_pointer_cast<HudGauge>(element)) {
				if (hGauge->GetId() == id) {
					hGauge->SetValue(gauge.GetValue());
					hGauge->SetLabel(gauge.GetLabel());
					hGauge->SetAlignment(gauge.GetAlignment());
					hGauge->SetPosition(gauge.GetPosition());
					hGauge->SetSize(gauge.GetSize());
					return;
				}
			}
		}
	}

	void HudManager::RemoveGauge(int id) {
		m_elements.erase(
			std::remove_if(
				m_elements.begin(),
				m_elements.end(),
				[id](const std::shared_ptr<HudElement>& e) { return e->GetId() == id; }
			),
			m_elements.end()
		);
	}

	unsigned int HudManager::GetTextureId(const std::string& path) {
		return AssetManager::GetInstance().GetTexture(path);
	}

	// Implementation of HudElement Draw methods
	glm::vec2 HudManager::GetAlignmentPosition(HudAlignment alignment, glm::vec2 elementSize, glm::vec2 offset) {
		ImVec2 displaySize = ImGui::GetIO().DisplaySize;
		ImVec2 basePos;

		switch (alignment) {
		case HudAlignment::TOP_LEFT:
			basePos = {0, 0};
			break;
		case HudAlignment::TOP_CENTER:
			basePos = {(displaySize.x - elementSize.x) * 0.5f, 0};
			break;
		case HudAlignment::TOP_RIGHT:
			basePos = {displaySize.x - elementSize.x, 0};
			break;
		case HudAlignment::MIDDLE_LEFT:
			basePos = {0, (displaySize.y - elementSize.y) * 0.5f};
			break;
		case HudAlignment::MIDDLE_CENTER:
			basePos = {(displaySize.x - elementSize.x) * 0.5f, (displaySize.y - elementSize.y) * 0.5f};
			break;
		case HudAlignment::MIDDLE_RIGHT:
			basePos = {displaySize.x - elementSize.x, (displaySize.y - elementSize.y) * 0.5f};
			break;
		case HudAlignment::BOTTOM_LEFT:
			basePos = {0, displaySize.y - elementSize.y};
			break;
		case HudAlignment::BOTTOM_CENTER:
			basePos = {(displaySize.x - elementSize.x) * 0.5f, displaySize.y - elementSize.y};
			break;
		case HudAlignment::BOTTOM_RIGHT:
			basePos = {displaySize.x - elementSize.x, displaySize.y - elementSize.y};
			break;
		}

		return {basePos.x + offset.x, basePos.y + offset.y};
	}

	void HudIcon::Draw(HudManager& manager) {
		glm::vec2 size = m_size;
		glm::vec2 pos = HudManager::GetAlignmentPosition(m_alignment, size, m_position);

		ImGui::SetCursorPos({pos.x, pos.y});

		unsigned int textureId = manager.GetTextureId(m_texture_path);
		if (textureId != 0) {
			ImVec4 tint_col = m_highlighted ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
			ImGui::Image(
				(void*)(intptr_t)textureId,
				{size.x, size.y},
				ImVec2(0, 0),
				ImVec2(1, 1),
				tint_col,
				ImVec4(0, 0, 0, 0)
			);
		}
	}

	void HudNumber::Draw(HudManager& /*manager*/) {
		char buffer[64];
		snprintf(
			buffer,
			sizeof(buffer),
			("%s: %." + std::to_string(m_precision) + "f").c_str(),
			m_label.c_str(),
			m_value
		);

		ImVec2    textSize = ImGui::CalcTextSize(buffer);
		glm::vec2 pos = HudManager::GetAlignmentPosition(m_alignment, {textSize.x, textSize.y}, m_position);

		ImGui::SetCursorPos({pos.x, pos.y});
		ImGui::Text("%s", buffer);
	}

	void HudGauge::Draw(HudManager& /*manager*/) {
		glm::vec2 size = m_size;
		glm::vec2 pos = HudManager::GetAlignmentPosition(m_alignment, size, m_position);

		ImGui::SetCursorPos({pos.x, pos.y});
		ImGui::ProgressBar(m_value, {size.x, size.y}, m_label.c_str());
	}

	void HudCompass::Update(float /*dt*/, const Camera& camera) {
		m_yaw = camera.yaw;
	}

	void HudCompass::Draw(HudManager& /*manager*/) {
		float yaw = m_yaw;
		while (yaw < 0)
			yaw += 360.0f;
		while (yaw >= 360.0f)
			yaw -= 360.0f;

		const char* directions[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
		int         index = static_cast<int>((yaw + 22.5f) / 45.0f) % 8;

		char buffer[32];
		snprintf(buffer, sizeof(buffer), "%s (%.0f*)", directions[index], yaw);

		ImVec2    textSize = ImGui::CalcTextSize(buffer);
		glm::vec2 pos = HudManager::GetAlignmentPosition(m_alignment, {textSize.x, textSize.y}, m_position);

		ImGui::SetCursorPos({pos.x, pos.y});
		ImGui::Text("%s", buffer);
	}

	void HudLocation::Update(float /*dt*/, const Camera& camera) {
		m_cameraPos = glm::vec3(camera.x, camera.y, camera.z);
	}

	void HudLocation::Draw(HudManager& /*manager*/) {
		char buffer[64];
		snprintf(buffer, sizeof(buffer), "Pos: %.1f, %.1f, %.1f", m_cameraPos.x, m_cameraPos.y, m_cameraPos.z);

		ImVec2    textSize = ImGui::CalcTextSize(buffer);
		glm::vec2 pos = HudManager::GetAlignmentPosition(m_alignment, {textSize.x, textSize.y}, m_position);

		ImGui::SetCursorPos({pos.x, pos.y});
		ImGui::Text("%s", buffer);
	}

	void HudScore::Update(float dt, const Camera& /*camera*/) {
		for (auto it = m_changes.begin(); it != m_changes.end();) {
			it->lifetime -= dt;
			if (it->lifetime <= 0) {
				it = m_changes.erase(it);
			} else {
				++it;
			}
		}
	}

	void HudScore::AddScore(int delta, const std::string& label) {
		m_value += delta;
		m_changes.push_back({delta, label, 2.0f});
	}

	void HudScore::Draw(HudManager& /*manager*/) {
		char buffer[64];
		snprintf(buffer, sizeof(buffer), "Score: %d", m_value);

		ImVec2    textSize = ImGui::CalcTextSize(buffer);
		glm::vec2 pos = HudManager::GetAlignmentPosition(m_alignment, {textSize.x, textSize.y}, m_position);

		ImGui::SetCursorPos({pos.x, pos.y});
		ImGui::Text("%s", buffer);

		// Draw changes fading out
		float yOffset = textSize.y + 5.0f;
		for (const auto& change : m_changes) {
			char cBuffer[64];
			if (change.label.empty()) {
				snprintf(cBuffer, sizeof(cBuffer), "%+d", change.delta);
			} else {
				snprintf(cBuffer, sizeof(cBuffer), "%+d %s", change.delta, change.label.c_str());
			}

			float alpha = std::clamp(change.lifetime, 0.0f, 1.0f);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, alpha));

			ImVec2    cTextSize = ImGui::CalcTextSize(cBuffer);
			glm::vec2 cPos = HudManager::GetAlignmentPosition(
				m_alignment,
				{cTextSize.x, cTextSize.y},
				{m_position.x, m_position.y + yOffset}
			);
			ImGui::SetCursorPos({cPos.x, cPos.y});
			ImGui::Text("%s", cBuffer);
			ImGui::PopStyleColor();

			yOffset += cTextSize.y + 2.0f;
		}
	}

	void HudIconSet::Draw(HudManager& manager) {
		for (size_t i = 0; i < m_texture_paths.size(); ++i) {
			glm::vec2 size = m_icon_size;
			// Horizontal layout
			float     xOffset = i * (m_icon_size.x + m_spacing);
			glm::vec2 pos = HudManager::GetAlignmentPosition(m_alignment, size, {m_position.x + xOffset, m_position.y});

			ImGui::SetCursorPos({pos.x, pos.y});

			unsigned int textureId = manager.GetTextureId(m_texture_paths[i]);
			if (textureId != 0) {
				bool   isSelected = (static_cast<int>(i) == m_selectedIndex);
				ImVec4 tint_col = isSelected ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
				ImGui::Image(
					(void*)(intptr_t)textureId,
					{size.x, size.y},
					ImVec2(0, 0),
					ImVec2(1, 1),
					tint_col,
					ImVec4(0, 0, 0, 0)
				);

				if (isSelected) {
					ImVec2 p_min = ImGui::GetItemRectMin();
					ImVec2 p_max = ImGui::GetItemRectMax();
					ImGui::GetWindowDrawList()->AddRect(p_min, p_max, IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.0f);
				}
			}
		}
	}

} // namespace Boidsish
