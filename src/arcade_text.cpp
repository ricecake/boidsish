#include "arcade_text.h"

#include <cmath>

#include "shader.h"
#include <GL/glew.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {

	ArcadeText::ArcadeText(
		const std::string& text,
		const std::string& font_path,
		float              font_size,
		float              depth,
		const glm::vec3&   position,
		float              radius,
		float              angle_degrees,
		const glm::vec3&   wrap_normal,
		const glm::vec3&   text_normal,
		float              duration
	):
		CurvedText(
			text,
			font_path,
			font_size,
			depth,
			position,
			radius,
			angle_degrees,
			wrap_normal,
			text_normal,
			duration
		),
		initial_position_(position) {}

	void ArcadeText::Update(float delta_time) {
		CurvedText::Update(delta_time);
		time_ += delta_time;

		if (rotation_speed_ != 0.0f) {
			rotation_angle_ += rotation_speed_ * delta_time;
		}

		if (pulse_speed_ != 0.0f) {
			float scale_val = 1.0f + sin(time_ * pulse_speed_) * pulse_amplitude_;
			SetScale(glm::vec3(scale_val));
		}

		if (bounce_speed_ != 0.0f) {
			float y_offset = sin(time_ * bounce_speed_) * bounce_amplitude_;
			SetPosition(initial_position_.x, initial_position_.y + y_offset, initial_position_.z);
		}
	}

	void ArcadeText::render() const {
		if (!allocation_.valid || shader == nullptr)
			return;

		if (double_copy_) {
			// First copy
			glm::mat4 m1 = GetModelMatrix();
			m1 = glm::rotate(m1, rotation_angle_, rotation_axis_);
			Text::render(*shader, m1);

			// Second copy (180 degrees offset)
			glm::mat4 m2 = GetModelMatrix();
			m2 = glm::rotate(m2, rotation_angle_ + glm::pi<float>(), rotation_axis_);
			Text::render(*shader, m2);
		} else {
			glm::mat4 model = GetModelMatrix();
			if (rotation_angle_ != 0.0f) {
				model = glm::rotate(model, rotation_angle_, rotation_axis_);
			}
			Text::render(*shader, model);
		}
	}

	void ArcadeText::OnPreRender(Shader& shader) const {
		Text::OnPreRender(shader);
		shader.setBool("isArcadeText", true);
		shader.setInt("arcadeWaveMode", static_cast<int>(wave_mode_));
		shader.setFloat("arcadeWaveAmplitude", wave_amplitude_);
		shader.setFloat("arcadeWaveFrequency", wave_frequency_);
		shader.setFloat("arcadeWaveSpeed", wave_speed_);

		shader.setBool("arcadeRainbowEnabled", rainbow_enabled_);
		shader.setFloat("arcadeRainbowSpeed", rainbow_speed_);
		shader.setFloat("arcadeRainbowFrequency", rainbow_frequency_);
	}

	void ArcadeText::GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const {
		MeshInfo mesh = GetMeshInfo(context.megabuffer);
		if (mesh.vao == 0)
			return;

		auto create_packet = [&](const glm::mat4& model) {
			RenderPacket packet;
			PopulatePacket(packet, mesh, context);

			packet.uniforms.model = model;
			packet.uniforms.is_text_effect = is_text_effect_ ? 1 : 0;
			packet.uniforms.text_fade_progress = text_fade_progress_;
			packet.uniforms.text_fade_softness = text_fade_softness_;
			packet.uniforms.text_fade_mode = text_fade_mode_;

			packet.uniforms.is_arcade_text = 1;
			packet.uniforms.arcade_wave_mode = static_cast<int>(wave_mode_);
			packet.uniforms.arcade_wave_amplitude = wave_amplitude_;
			packet.uniforms.arcade_wave_frequency = wave_frequency_;
			packet.uniforms.arcade_wave_speed = wave_speed_;
			packet.uniforms.arcade_rainbow_enabled = rainbow_enabled_ ? 1 : 0;
			packet.uniforms.arcade_rainbow_speed = rainbow_speed_;
			packet.uniforms.arcade_rainbow_frequency = rainbow_frequency_;

			// Recalculate sort key because we changed model (for depth)
			RenderLayer layer = RenderLayer::Transparent;
			float       normalized_depth = context.CalculateNormalizedDepth(glm::vec3(model[3]));
			packet.sort_key = CalculateSortKey(
				layer,
				packet.shader_handle,
				packet.vao,
				packet.draw_mode,
				packet.index_count > 0,
				packet.material_handle,
				normalized_depth
			);

			return packet;
		};

		if (double_copy_) {
			// First copy
			glm::mat4 m1 = GetModelMatrix();
			m1 = glm::rotate(m1, rotation_angle_, rotation_axis_);
			out_packets.push_back(create_packet(m1));

			// Second copy (180 degrees offset)
			glm::mat4 m2 = GetModelMatrix();
			m2 = glm::rotate(m2, rotation_angle_ + glm::pi<float>(), rotation_axis_);
			out_packets.push_back(create_packet(m2));
		} else {
			glm::mat4 model = GetModelMatrix();
			if (rotation_angle_ != 0.0f) {
				model = glm::rotate(model, rotation_angle_, rotation_axis_);
			}
			out_packets.push_back(create_packet(model));
		}
	}

} // namespace Boidsish
