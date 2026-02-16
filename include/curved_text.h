#pragma once

#include "text.h"

namespace Boidsish {

	class CurvedText: public Text {
	public:
		CurvedText(
			const std::string& text,
			const std::string& font_path,
			float              font_size,
			float              depth,
			const glm::vec3&   position,
			float              radius,
			float              angle_degrees,
			const glm::vec3&   wrap_normal,
			const glm::vec3&   text_normal,
			float              duration = 5.0f
		);

		void Update(float delta_time) override;

		bool IsExpired() const override { return age_ >= total_duration_; }

		using Text::render;
		void render() const override;

	protected:
		void GenerateMesh(const std::string& text, float font_size, float depth) override;

	private:
		glm::vec3 center_;
		float     radius_;
		float     angle_rad_;
		glm::vec3 wrap_normal_;
		glm::vec3 text_normal_;
		float     total_duration_;
		float     age_ = 0.0f;

		float fade_in_time_ = 1.0f;
		float fade_out_time_ = 1.0f;
	};

} // namespace Boidsish
