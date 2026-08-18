#ifndef VOLUMETRIC_FIRE_SMOKE_MANAGER_H
#define VOLUMETRIC_FIRE_SMOKE_MANAGER_H

#include <memory>
#include <vector>
#include <string>

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "IManager.h"
#include "frustum.h"

class ComputeShader;
class ShaderBase;

namespace Boidsish {

	class ServiceLocator;

	class VolumetricFireSmokeManager : public IManager {
	public:
		VolumetricFireSmokeManager(ServiceLocator& loc);
		VolumetricFireSmokeManager();
		~VolumetricFireSmokeManager() override;

		void Initialize() override;

		/**
		 * @brief Updates the fire/smoke simulation and bakes the 3D volume if visible in frustum.
		 */
		void Update(
			float deltaTime,
			float time,
			const Frustum& cameraFrustum,
			const glm::vec3& windVel = glm::vec3(0.0f)
		);

		/**
		 * @brief Binds local 3D volume texture, effect map, and uniforms to a shader (e.g. volumetric_injection.comp).
		 */
		void BindToShader(ShaderBase& shader);

		/**
		 * @brief Triggers ignition at a world position.
		 */
		void Ignite(const glm::vec3& worldPos, float radius = 5.0f, float intensity = 1.0f);

		/**
		 * @brief Seeds or modifies fuel on the effect coverage map.
		 */
		void SeedFuel(const glm::vec3& worldPos, float radius = 10.0f, float fuelAmount = 1.0f);

		/**
		 * @brief Resets the effect coverage map with uniform fuel.
		 */
		void ResetEffectMap(float defaultFuel = 1.0f);

		// Visibility & Frustum Culling
		bool IsInFrustum(const Frustum& frustum) const;
		bool IsActive() const { return enabled_ && (activeFireCount_ > 0 || hasActiveSmoke_); }

		// Configuration
		void SetEnabled(bool enabled) { enabled_ = enabled; }
		bool IsEnabled() const { return enabled_; }

		void SetWorldBounds(const glm::vec3& center, const glm::vec3& extents);
		glm::vec3 GetWorldCenter() const { return centerPosition_; }
		glm::vec3 GetWorldExtents() const { return volumeExtents_; }

		void SetSpreadRate(float rate) { spreadRate_ = rate; }
		float GetSpreadRate() const { return spreadRate_; }

		void SetBuoyancy(float buoyancy) { buoyancy_ = buoyancy; }
		float GetBuoyancy() const { return buoyancy_; }

		void SetSmokeDensityScale(float scale) { smokeDensityScale_ = scale; }
		float GetSmokeDensityScale() const { return smokeDensityScale_; }

		void SetFireIntensityScale(float scale) { fireIntensityScale_ = scale; }
		float GetFireIntensityScale() const { return fireIntensityScale_; }

		void SetFlameColor(const glm::vec3& color) { flameColor_ = color; }
		glm::vec3 GetFlameColor() const { return flameColor_; }

		void SetBlackbodyMultiplier(float mult) { blackbodyMultiplier_ = mult; }
		float GetBlackbodyMultiplier() const { return blackbodyMultiplier_; }

		void SetFireOpacityScale(float opacity) { fireOpacityScale_ = opacity; }
		float GetFireOpacityScale() const { return fireOpacityScale_; }

		void SetTemperatureScale(float scale) { temperatureScale_ = scale; }
		float GetTemperatureScale() const { return temperatureScale_; }

		GLuint GetVolume3DTexture() const { return volume3DTex_; }
		GLuint GetEffectMapTexture() const { return effectMapTex_[pingPongIndex_]; }

	private:
		void CreateTextures();
		void CreateShaders();

		GLuint effectMapTex_[2] = { 0, 0 }; // 512x512 RGBA32F ping-pong effect map
		int pingPongIndex_ = 0;

		GLuint volume3DTex_ = 0;             // 128x128x128 RGBA16F 3D volume texture

		std::unique_ptr<ComputeShader> simShader_;
		std::unique_ptr<ComputeShader> bakeShader_;

		glm::vec3 centerPosition_ = glm::vec3(0.0f, 15.0f, 0.0f);
		glm::vec3 volumeExtents_ = glm::vec3(120.0f, 60.0f, 120.0f);

		bool enabled_ = true;
		int activeFireCount_ = 1;
		bool hasActiveSmoke_ = true;

		float spreadRate_ = 0.8f;
		float buoyancy_ = 2.5f;
		float smokeDensityScale_ = 1.2f;
		float fireIntensityScale_ = 5.0f;
		float blackbodyMultiplier_ = 12.0f;
		float fireOpacityScale_ = 10.0f;
		float temperatureScale_ = 1.0f;
		glm::vec3 flameColor_ = glm::vec3(1.0f, 0.5f, 0.1f);

		float lastSimTime_ = 0.0f;
		int simFrame_ = 0;
	};

} // namespace Boidsish

#endif // VOLUMETRIC_FIRE_SMOKE_MANAGER_H
