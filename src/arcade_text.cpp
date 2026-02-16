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
		if (vao_ == 0 || shader == nullptr)
			return;

		shader->use();
		shader->setBool("isArcadeText", true);
		shader->setInt("arcadeWaveMode", static_cast<int>(wave_mode_));
		shader->setFloat("arcadeWaveAmplitude", wave_amplitude_);
		shader->setFloat("arcadeWaveFrequency", wave_frequency_);
		shader->setFloat("arcadeWaveSpeed", wave_speed_);

		shader->setBool("arcadeRainbowEnabled", rainbow_enabled_);
		shader->setFloat("arcadeRainbowSpeed", rainbow_speed_);
		shader->setFloat("arcadeRainbowFrequency", rainbow_frequency_);

		if (double_copy_) {
			// First copy
			glm::mat4 m1 = GetModelMatrix();
			m1 = glm::rotate(m1, rotation_angle_, rotation_axis_);
			render_internal(m1);

			// Second copy (180 degrees offset)
			glm::mat4 m2 = GetModelMatrix();
			m2 = glm::rotate(m2, rotation_angle_ + glm::pi<float>(), rotation_axis_);
			render_internal(m2);
		} else {
			glm::mat4 model = GetModelMatrix();
			if (rotation_angle_ != 0.0f) {
				model = glm::rotate(model, rotation_angle_, rotation_axis_);
			}
			render_internal(model);
		}

		// Reset arcade and text effect flags for subsequent renders
		shader->setBool("isArcadeText", false);
		shader->setBool("isTextEffect", false);
	}

	void ArcadeText::render_internal(const glm::mat4& model_matrix) const {
		shader->setMat4("model", model_matrix);
		shader->setVec3("objectColor", GetR(), GetG(), GetB());
		shader->setFloat("objectAlpha", GetA());
		shader->setBool("use_texture", false);

		shader->setBool("isTextEffect", is_text_effect_);
		if (is_text_effect_) {
			shader->setFloat("textFadeProgress", text_fade_progress_);
			shader->setFloat("textFadeSoftness", text_fade_softness_);
			shader->setInt("textFadeMode", text_fade_mode_);
		}

		// Set PBR material properties
		shader->setBool("usePBR", UsePBR());
		if (UsePBR()) {
			shader->setFloat("roughness", GetRoughness());
			shader->setFloat("metallic", GetMetallic());
			shader->setFloat("ao", GetAO());
		}

		glBindVertexArray(vao_);
		glDrawArrays(GL_TRIANGLES, 0, vertex_count_);
		glBindVertexArray(0);
	}

	void ArcadeText::GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const {
		if (vao_ == 0 || vertex_count_ == 0) return;

		glm::mat4 model_matrix = GetModelMatrix();
		glm::vec3 world_pos = glm::vec3(model_matrix[3]);

		RenderPacket packet;
		packet.vao = vao_;
		packet.vbo = vbo_;
		packet.vertex_count = static_cast<unsigned int>(vertex_count_);
		packet.draw_mode = GL_TRIANGLES;
		packet.index_type = 0;
		packet.shader_id = shader ? shader->ID : 0;

		packet.uniforms.model = model_matrix;
		packet.uniforms.color = glm::vec4(GetR(), GetG(), GetB(), GetA());
		packet.uniforms.use_pbr = UsePBR();
		packet.uniforms.roughness = GetRoughness();
		packet.uniforms.metallic = GetMetallic();
		packet.uniforms.ao = GetAO();
		packet.uniforms.use_texture = false;
		packet.uniforms.is_instanced = IsInstanced();
		packet.uniforms.is_colossal = IsColossal();

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

		packet.casts_shadows = CastsShadows();

		RenderLayer layer = RenderLayer::Transparent;

		packet.shader_handle = shader_handle;
		packet.material_handle = MaterialHandle(0);

		float normalized_depth = context.CalculateNormalizedDepth(world_pos);
		packet.sort_key = CalculateSortKey(layer, packet.shader_handle, packet.material_handle, normalized_depth);

		out_packets.push_back(packet);
	}

} // namespace Boidsish
