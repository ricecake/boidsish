#include "volumetric_fire_smoke_manager.h"

#include <iostream>
#include <algorithm>
#include <vector>

#include "service_locator.h"
#include "shader.h"
#include "logger.h"

namespace Boidsish {

	VolumetricFireSmokeManager::VolumetricFireSmokeManager(ServiceLocator& /*loc*/) {
	}

	VolumetricFireSmokeManager::VolumetricFireSmokeManager() {
	}

	VolumetricFireSmokeManager::~VolumetricFireSmokeManager() {
		if (effectMapTex_[0]) glDeleteTextures(2, effectMapTex_);
		if (volume3DTex_) glDeleteTextures(1, &volume3DTex_);
	}

	void VolumetricFireSmokeManager::Initialize() {
		CreateTextures();
		CreateShaders();
		ResetEffectMap(1.0f); // Initialize with burnable fuel
		// Ignite initial spark near center to start dynamic combustion
		Ignite(centerPosition_ + glm::vec3(0.0f, -volumeExtents_.y * 0.4f, 0.0f), 8.0f, 1.0f);
	}

	void VolumetricFireSmokeManager::CreateTextures() {
		// 1. Create 2D Ping-Pong Effect Maps (512x512 RGBA32F)
		// Channel R: Fuel (0..1)
		// Channel G: Active Fire (0..1)
		// Channel B: Smoke Density (0..1)
		// Channel A: Temperature / Heat (0..1000)
		if (effectMapTex_[0]) glDeleteTextures(2, effectMapTex_);
		glGenTextures(2, effectMapTex_);

		for (int i = 0; i < 2; ++i) {
			glBindTexture(GL_TEXTURE_2D, effectMapTex_[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 512, 512, 0, GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		glBindTexture(GL_TEXTURE_2D, 0);

		// 2. Create 3D Volumetric Texture (128x128x128 RGBA16F)
		// Channel R: Fire Emissive / Glow
		// Channel G: Smoke Extinction / Density
		// Channel B: Flame Temperature
		// Channel A: Detail Noise / Turbulence
		if (volume3DTex_) glDeleteTextures(1, &volume3DTex_);
		glGenTextures(1, &volume3DTex_);
		glBindTexture(GL_TEXTURE_3D, volume3DTex_);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, 128, 128, 128, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_3D, 0);
	}

	void VolumetricFireSmokeManager::CreateShaders() {
		simShader_ = std::make_unique<ComputeShader>("shaders/effects/local_fire_smoke_sim.comp");
		bakeShader_ = std::make_unique<ComputeShader>("shaders/effects/local_fire_smoke_bake.comp");

		if (!simShader_->isValid()) {
			logger::WARNING("Local fire/smoke simulation compute shader failed to compile!");
		}
		if (!bakeShader_->isValid()) {
			logger::WARNING("Local fire/smoke volume bake compute shader failed to compile!");
		}
	}

	bool VolumetricFireSmokeManager::IsInFrustum(const Frustum& frustum) const {
		glm::vec3 minBounds = centerPosition_ - volumeExtents_ * 0.5f;
		glm::vec3 maxBounds = centerPosition_ + volumeExtents_ * 0.5f;
		return frustum.IsBoxInFrustum(minBounds, maxBounds);
	}

	void VolumetricFireSmokeManager::Update(
		float deltaTime,
		float time,
		const Frustum& cameraFrustum,
		const glm::vec3& windVel
	) {
		if (!enabled_) return;

		// 1. Frustum Check: Only execute compute shaders if the volumetric box is visible in camera frustum!
		if (!IsInFrustum(cameraFrustum)) {
			return;
		}

		simFrame_++;

		// 2. Dispatch 2D Combustion Simulation Compute Shader
		if (simShader_ && simShader_->isValid()) {
			int readIdx = pingPongIndex_;
			int writeIdx = 1 - pingPongIndex_;

			simShader_->use();
			simShader_->setFloat("uDeltaTime", deltaTime);
			simShader_->setFloat("uTime", time);
			simShader_->setFloat("uSpreadRate", spreadRate_);
			simShader_->setVec3("uWindVelocity", windVel);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, effectMapTex_[readIdx]);
			simShader_->setInt("uInEffectMap", 0);

			glBindImageTexture(0, effectMapTex_[writeIdx], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

			glDispatchCompute((512 + 15) / 16, (512 + 15) / 16, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

			pingPongIndex_ = writeIdx;
		}

		// 3. Dispatch 3D Local Volumetric Texture Bake Compute Shader
		if (bakeShader_ && bakeShader_->isValid()) {
			bakeShader_->use();
			bakeShader_->setFloat("uTime", time);
			bakeShader_->setVec3("uCenterPosition", centerPosition_);
			bakeShader_->setVec3("uVolumeExtents", volumeExtents_);
			bakeShader_->setFloat("uBuoyancy", buoyancy_);
			bakeShader_->setFloat("uSmokeDensityScale", smokeDensityScale_);
			bakeShader_->setFloat("uFireIntensityScale", fireIntensityScale_);
			bakeShader_->setVec3("uFlameColor", flameColor_);
			bakeShader_->setVec3("uWindVelocity", windVel);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, effectMapTex_[pingPongIndex_]);
			bakeShader_->setInt("uEffectMap", 0);

			glBindImageTexture(1, volume3DTex_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);

			glDispatchCompute((128 + 3) / 4, (128 + 3) / 4, (128 + 3) / 4);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
		}
	}

	void VolumetricFireSmokeManager::BindToShader(ShaderBase& shader) {
		shader.use();

		glm::vec3 minBounds = centerPosition_ - volumeExtents_ * 0.5f;
		glm::vec3 maxBounds = centerPosition_ + volumeExtents_ * 0.5f;

		shader.setVec3("u_localVolMin", minBounds);
		shader.setVec3("u_localVolMax", maxBounds);
		shader.setVec3("u_localVolCenter", centerPosition_);
		shader.setVec3("u_localVolExtents", volumeExtents_);
		shader.setBool("u_localVolActive", enabled_);

		shader.setFloat("u_localVolSmokeScale", smokeDensityScale_);
		shader.setFloat("u_localVolFireScale", fireIntensityScale_);
		shader.setVec3("u_localVolFlameColor", flameColor_);

		// Bind 3D Volume Texture
		glActiveTexture(GL_TEXTURE0 + 54); // Unit 54 for local 3D volume
		glBindTexture(GL_TEXTURE_3D, volume3DTex_);
		shader.trySetInt("u_fireSmoke3DTexture", 54);

		// Bind 2D Effect Map Texture
		glActiveTexture(GL_TEXTURE0 + 55); // Unit 55 for effect map
		glBindTexture(GL_TEXTURE_2D, effectMapTex_[pingPongIndex_]);
		shader.trySetInt("u_fireSmokeEffectMap", 55);
	}

	void VolumetricFireSmokeManager::Ignite(const glm::vec3& worldPos, float radius, float intensity) {
		// Calculate UV on 2D effect map from world position
		glm::vec3 relPos = (worldPos - centerPosition_) / volumeExtents_ + glm::vec3(0.5f);
		if (relPos.x < 0.0f || relPos.x > 1.0f || relPos.z < 0.0f || relPos.z > 1.0f) return;

		int cx = static_cast<int>(relPos.x * 512.0f);
		int cy = static_cast<int>(relPos.z * 512.0f);
		int r = static_cast<int>((radius / volumeExtents_.x) * 512.0f);
		r = std::max(1, r);

		std::vector<glm::vec4> currentPixels(512 * 512);
		glBindTexture(GL_TEXTURE_2D, effectMapTex_[pingPongIndex_]);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, currentPixels.data());

		for (int dy = -r; dy <= r; ++dy) {
			for (int dx = -r; dx <= r; ++dx) {
				int px = cx + dx;
				int py = cy + dy;
				if (px >= 0 && px < 512 && py >= 0 && py < 512) {
					float dist = sqrt(float(dx * dx + dy * dy)) / float(r);
					if (dist <= 1.0f) {
						float factor = (1.0f - dist) * intensity;
						int idx = py * 512 + px;
						currentPixels[idx].g = std::clamp(currentPixels[idx].g + factor, 0.0f, 1.0f); // Fire
						currentPixels[idx].a = std::max(currentPixels[idx].a, 800.0f * factor);        // Temperature
					}
				}
			}
		}

		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 512, 512, GL_RGBA, GL_FLOAT, currentPixels.data());
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void VolumetricFireSmokeManager::SeedFuel(const glm::vec3& worldPos, float radius, float fuelAmount) {
		glm::vec3 relPos = (worldPos - centerPosition_) / volumeExtents_ + glm::vec3(0.5f);
		if (relPos.x < 0.0f || relPos.x > 1.0f || relPos.z < 0.0f || relPos.z > 1.0f) return;

		int cx = static_cast<int>(relPos.x * 512.0f);
		int cy = static_cast<int>(relPos.z * 512.0f);
		int r = static_cast<int>((radius / volumeExtents_.x) * 512.0f);
		r = std::max(1, r);

		std::vector<glm::vec4> currentPixels(512 * 512);
		glBindTexture(GL_TEXTURE_2D, effectMapTex_[pingPongIndex_]);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, currentPixels.data());

		for (int dy = -r; dy <= r; ++dy) {
			for (int dx = -r; dx <= r; ++dx) {
				int px = cx + dx;
				int py = cy + dy;
				if (px >= 0 && px < 512 && py >= 0 && py < 512) {
					float dist = sqrt(float(dx * dx + dy * dy)) / float(r);
					if (dist <= 1.0f) {
						float factor = (1.0f - dist) * fuelAmount;
						int idx = py * 512 + px;
						currentPixels[idx].r = std::clamp(currentPixels[idx].r + factor, 0.0f, 1.0f);
					}
				}
			}
		}

		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 512, 512, GL_RGBA, GL_FLOAT, currentPixels.data());
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void VolumetricFireSmokeManager::ResetEffectMap(float defaultFuel) {
		std::vector<glm::vec4> pixels(512 * 512, glm::vec4(defaultFuel, 0.0f, 0.0f, 0.0f));

		for (int i = 0; i < 2; ++i) {
			glBindTexture(GL_TEXTURE_2D, effectMapTex_[i]);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 512, 512, GL_RGBA, GL_FLOAT, pixels.data());
		}
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void VolumetricFireSmokeManager::SetWorldBounds(const glm::vec3& center, const glm::vec3& extents) {
		centerPosition_ = center;
		volumeExtents_ = extents;
	}

} // namespace Boidsish
