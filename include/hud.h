#pragma once

#include <string>

#include <glm/glm.hpp>

namespace Boidsish {

	enum class HudAlignment {
		TOP_LEFT,
		TOP_CENTER,
		TOP_RIGHT,
		MIDDLE_LEFT,
		MIDDLE_CENTER,
		MIDDLE_RIGHT,
		BOTTOM_LEFT,
		BOTTOM_CENTER,
		BOTTOM_RIGHT
	};

	class HudManager;
	struct Camera;

	class HudElement {
	public:
		HudElement(HudAlignment alignment = HudAlignment::TOP_LEFT, glm::vec2 position = {0, 0}):
			m_alignment(alignment), m_position(position) {}
		virtual ~HudElement() = default;

		virtual void Update(float dt, const Camera& camera) {}
		virtual void Draw(HudManager& manager) = 0;

		void         SetAlignment(HudAlignment alignment) { m_alignment = alignment; }
		HudAlignment GetAlignment() const { return m_alignment; }

		void      SetPosition(glm::vec2 position) { m_position = position; }
		glm::vec2 GetPosition() const { return m_position; }

		void SetVisible(bool visible) { m_visible = visible; }
		bool IsVisible() const { return m_visible; }

		// Legacy ID support
		void SetId(int id) { m_id = id; }
		int  GetId() const { return m_id; }

	protected:
		HudAlignment m_alignment;
		glm::vec2    m_position;
		bool         m_visible = true;
		int          m_id = -1;
	};

	class HudIcon : public HudElement {
	public:
		HudIcon(
			const std::string& texture_path,
			HudAlignment       alignment = HudAlignment::TOP_LEFT,
			glm::vec2          position = {0, 0},
			glm::vec2          size = {64, 64}
		):
			HudElement(alignment, position), m_texture_path(texture_path), m_size(size) {}

		// Legacy compatibility constructor
		HudIcon(
			int                id,
			const std::string& texture_path,
			HudAlignment       alignment,
			glm::vec2          position,
			glm::vec2          size,
			bool               highlighted = false
		):
			HudElement(alignment, position), m_texture_path(texture_path), m_size(size), m_highlighted(highlighted) {
			m_id = id;
		}

		void Draw(HudManager& manager) override;

		void               SetTexturePath(const std::string& path) { m_texture_path = path; }
		const std::string& GetTexturePath() const { return m_texture_path; }

		void      SetSize(glm::vec2 size) { m_size = size; }
		glm::vec2 GetSize() const { return m_size; }

		void SetHighlighted(bool highlighted) { m_highlighted = highlighted; }
		bool IsHighlighted() const { return m_highlighted; }

	private:
		std::string m_texture_path;
		glm::vec2   m_size;
		bool        m_highlighted = false;
	};

	class HudNumber : public HudElement {
	public:
		HudNumber(
			float              value = 0.0f,
			const std::string& label = "",
			HudAlignment       alignment = HudAlignment::TOP_RIGHT,
			glm::vec2          position = {-10, 10},
			int                precision = 2
		):
			HudElement(alignment, position), m_value(value), m_label(label), m_precision(precision) {}

		// Legacy compatibility constructor
		HudNumber(
			int                id,
			float              value,
			const std::string& label,
			HudAlignment       alignment,
			glm::vec2          position,
			int                precision = 0
		):
			HudElement(alignment, position), m_value(value), m_label(label), m_precision(precision) {
			m_id = id;
		}

		void Draw(HudManager& manager) override;

		void  SetValue(float value) { m_value = value; }
		float GetValue() const { return m_value; }

		void               SetLabel(const std::string& label) { m_label = label; }
		const std::string& GetLabel() const { return m_label; }

		void SetPrecision(int precision) { m_precision = precision; }
		int  GetPrecision() const { return m_precision; }

	private:
		float       m_value;
		std::string m_label;
		int         m_precision;
	};

	class HudGauge : public HudElement {
	public:
		HudGauge(
			float              value = 0.0f,
			const std::string& label = "",
			HudAlignment       alignment = HudAlignment::BOTTOM_CENTER,
			glm::vec2          position = {0, -50},
			glm::vec2          size = {200, 20}
		):
			HudElement(alignment, position), m_value(value), m_label(label), m_size(size) {}

		// Legacy compatibility constructor
		HudGauge(
			int                id,
			float              value,
			const std::string& label,
			HudAlignment       alignment,
			glm::vec2          position,
			glm::vec2          size
		):
			HudElement(alignment, position), m_value(value), m_label(label), m_size(size) {
			m_id = id;
		}

		void Draw(HudManager& manager) override;

		void  SetValue(float value) { m_value = value; } // Should be [0, 1]
		float GetValue() const { return m_value; }

		void               SetLabel(const std::string& label) { m_label = label; }
		const std::string& GetLabel() const { return m_label; }

		void      SetSize(glm::vec2 size) { m_size = size; }
		glm::vec2 GetSize() const { return m_size; }

	private:
		float       m_value;
		std::string m_label;
		glm::vec2   m_size;
	};

	class HudCompass : public HudElement {
	public:
		HudCompass(HudAlignment alignment = HudAlignment::TOP_CENTER, glm::vec2 position = {0, 20}):
			HudElement(alignment, position) {}

		void Update(float dt, const Camera& camera) override;
		void Draw(HudManager& manager) override;

	private:
		float m_yaw = 0.0f;
	};

	class HudLocation : public HudElement {
	public:
		HudLocation(HudAlignment alignment = HudAlignment::BOTTOM_LEFT, glm::vec2 position = {10, -10}):
			HudElement(alignment, position) {}

		void Update(float dt, const Camera& camera) override;
		void Draw(HudManager& manager) override;

	private:
		glm::vec3 m_cameraPos{0.0f};
	};

	class HudScore : public HudElement {
	public:
		HudScore(HudAlignment alignment = HudAlignment::TOP_RIGHT, glm::vec2 position = {-10, 50}):
			HudElement(alignment, position) {}

		void Update(float dt, const Camera& camera) override;
		void Draw(HudManager& manager) override;

		void SetValue(int value) { m_value = value; }
		void AddScore(int delta, const std::string& label = "");

	private:
		int m_value = 0;
		struct ScoreChange {
			int         delta;
			std::string label;
			float       lifetime = 2.0f;
		};
		std::vector<ScoreChange> m_changes;
	};

	class HudIconSet : public HudElement {
	public:
		HudIconSet(
			const std::vector<std::string>& texture_paths,
			HudAlignment                    alignment = HudAlignment::TOP_LEFT,
			glm::vec2                       position = {10, 10},
			glm::vec2                       icon_size = {64, 64},
			float                           spacing = 10.0f
		):
			HudElement(alignment, position),
			m_texture_paths(texture_paths),
			m_icon_size(icon_size),
			m_spacing(spacing) {}

		void Draw(HudManager& manager) override;

		void SetSelectedIndex(int index) { m_selectedIndex = index; }
		int  GetSelectedIndex() const { return m_selectedIndex; }

	private:
		std::vector<std::string> m_texture_paths;
		glm::vec2                m_icon_size;
		float                    m_spacing;
		int                      m_selectedIndex = 0;
	};

} // namespace Boidsish
