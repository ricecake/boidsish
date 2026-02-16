#pragma once

#include "curved_text.h"

namespace Boidsish {

	enum class ArcadeWaveMode { NONE = 0, VERTICAL = 1, FLAG = 2, TWIST = 3 };

	class ArcadeText: public CurvedText {
	public:
		ArcadeText(
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
		void render() const override;

		void GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const override;

		// Arcade Effect Setters
		void SetWaveMode(ArcadeWaveMode mode) { wave_mode_ = mode; }

		void SetWaveAmplitude(float amp) { wave_amplitude_ = amp; }

		void SetWaveFrequency(float freq) { wave_frequency_ = freq; }

		void SetWaveSpeed(float speed) { wave_speed_ = speed; }

		void SetDoubleCopy(bool enabled) { double_copy_ = enabled; }

		void SetRotationSpeed(float speed) { rotation_speed_ = speed; }

		void SetRotationAxis(const glm::vec3& axis) { rotation_axis_ = axis; }

		void SetRainbowEnabled(bool enabled) { rainbow_enabled_ = enabled; }

		void SetRainbowSpeed(float speed) { rainbow_speed_ = speed; }

		void SetRainbowFrequency(float freq) { rainbow_frequency_ = freq; }

		void SetPulseSpeed(float speed) { pulse_speed_ = speed; }

		void SetPulseAmplitude(float amp) { pulse_amplitude_ = amp; }

		void SetBounceSpeed(float speed) { bounce_speed_ = speed; }

		void SetBounceAmplitude(float amp) { bounce_amplitude_ = amp; }

	private:
		ArcadeWaveMode wave_mode_ = ArcadeWaveMode::NONE;
		float          wave_amplitude_ = 0.5f;
		float          wave_frequency_ = 10.0f;
		float          wave_speed_ = 5.0f;

		bool      double_copy_ = false;
		float     rotation_speed_ = 0.0f;
		glm::vec3 rotation_axis_ = glm::vec3(0, 1, 0);
		float     rotation_angle_ = 0.0f;

		bool  rainbow_enabled_ = false;
		float rainbow_speed_ = 2.0f;
		float rainbow_frequency_ = 5.0f;

		float pulse_speed_ = 0.0f;
		float pulse_amplitude_ = 0.2f;

		float     bounce_speed_ = 0.0f;
		float     bounce_amplitude_ = 2.0f;
		glm::vec3 initial_position_;

		float time_ = 0.0f;

		void render_internal(const glm::mat4& model_matrix) const;
	};

} // namespace Boidsish
