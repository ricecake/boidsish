#ifndef VOLUMETRIC_LIGHTING_MANAGER_H
#define VOLUMETRIC_LIGHTING_MANAGER_H

#include <memory>
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "IManager.h"
#include "persistent_buffer.h"

class ComputeShader;

namespace Boidsish {

	class ServiceLocator;

	struct VolumetricLightingUbo {
		glm::vec4 cascadeRanges; // x=c0_near, y=c0_far, z=c1_near, w=c1_far
		glm::ivec4 cascadeRes;    // x,y,z resolution, w unused
		float intensity;
		float scatteringCoeff;
		float extinctionCoeff;
		float mieAnisotropy;
		glm::vec4 ambientFactor; // xyz = color, w = intensity
		float phaseG;
		glm::ivec4 weatherGridOriginSize; // x,z = origin, y = width, w = height
		float _padding[3];
	};

	class VolumetricLightingManager : public IManager {
	public:
		VolumetricLightingManager(ServiceLocator& loc);
		~VolumetricLightingManager();

		void Initialize() override;

		void Update(
			const glm::mat4& view,
			const glm::mat4& projection,
			const glm::vec3& cameraPos,
			float deltaTime,
			float time,
			GLuint weatherScalarTexture,
			const glm::ivec4& weatherGridOriginSize,
			const glm::vec3& aerosolColor
		);

		void AdvanceFrame();
		void BindTextures();

		// Parameters
		void SetIntensity(float i) { _intensity = i; }
		void SetScatteringCoeff(float s) { _scatteringCoeff = s; }
		void SetExtinctionCoeff(float e) { _extinctionCoeff = e; }

		GLuint GetIntegratedVolume(int cascade) const { return _integratedVolumes[cascade][_frameIndex]; }
		GLuint GetUboId() const { return _ubo->GetBufferId(); }
		size_t GetUboOffset() const { return _ubo->GetFrameOffset(); }
		size_t GetUboSize() const { return sizeof(VolumetricLightingUbo); }

	private:
		void CreateTextures();
		void CreateShaders();
		void UpdateUbo();

		ServiceLocator& _loc;

		static constexpr int kNumCascades = 2;
		static constexpr int kNumBuffers = 3;

		GLuint _scatteringVolumes[kNumCascades][kNumBuffers];
		GLuint _integratedVolumes[kNumCascades][kNumBuffers];
		int _frameIndex = 0;

		std::unique_ptr<ComputeShader> _densityShader;
		std::unique_ptr<ComputeShader> _injectionShader;
		std::unique_ptr<ComputeShader> _integrationShader;

		std::unique_ptr<PersistentBuffer<VolumetricLightingUbo>> _ubo;

		GLuint _weatherScalarTexture = 0;
		glm::ivec4 _weatherGridOriginSize = glm::ivec4(0);
		glm::vec3 _aerosolColor = glm::vec3(1.0f);

		float _intensity = 1.0f;
		float _scatteringCoeff = 0.1f;
		float _extinctionCoeff = 0.1f;
		float _mieAnisotropy = 0.7f;

		// Froxel Resolution
		int _resX = 128;
		int _resY = 72;
		int _resZ = 128;

		float _cascade0Far = 50.0f;
		float _cascade1Far = 2000.0f;
	};

} // namespace Boidsish

#endif // VOLUMETRIC_LIGHTING_MANAGER_H
