#ifndef VOLUMETRIC_LIGHTING_MANAGER_H
#define VOLUMETRIC_LIGHTING_MANAGER_H

#include <memory>
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "IManager.h"
#include "persistent_buffer.h"
#include "light.h"

class ComputeShader;

namespace Boidsish {

	class ServiceLocator;

	struct alignas(16) VolumetricLightingUbo {
		glm::vec4  cascadeRanges; // offset 0
		glm::ivec4 cascadeRes;    // offset 16
		float      intensity;       // offset 32
		float      scatteringCoeff; // offset 36
		float      extinctionCoeff; // offset 40
		float      mieAnisotropy;   // offset 44
		glm::vec4  ambientFactor;   // offset 48, xyz = color, w = intensity
		float      phaseG;         // offset 64
		float      time;           // offset 68
		float      noiseScale;     // offset 72
		float      noiseStrength;  // offset 76
		float      globalAerosol;  // offset 80
		float      globalHumidity; // offset 84
		float      _pad1, _pad2;   // offset 88, 92 - Align to 16 bytes for next vector
		glm::ivec4 weatherGridOriginSize; // offset 96, x,z = origin, y = width, w = height
		glm::vec4  _padding2;             // offset 112, Final padding for 16-byte alignment
	}; // Total: 128 bytes

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
			const glm::vec3& aerosolColor,
			const std::vector<Light>& allLights
		);

		void AdvanceFrame();
		void BindTextures();

		// Parameters
		void SetIntensity(float i) { _intensity = i; }

		GLuint GetIntegratedVolume(int cascade) const { return _integratedVolumes[cascade][_frameIndex]; }
		GLuint GetUboId() const { return _ubo->GetBufferId(); }
		size_t GetUboOffset() const { return _ubo->GetFrameOffset(); }
		size_t GetUboSize() const { return sizeof(VolumetricLightingUbo); }

	private:
		void CreateTextures();
		void CreateShaders();
		void UpdateUbo();

		ServiceLocator& _loc;

		static constexpr int kNumCascades = 4;
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

		// Froxel Resolution
		int _resX = 128;
		int _resY = 72;
		int _resZ = 128;

		float _cascade0Far = 20.0f;
		float _cascade1Far = 80.0f;
		float _cascade2Far = 350.0f;
		float _cascade3Far = 2000.0f;
	};

} // namespace Boidsish

#endif // VOLUMETRIC_LIGHTING_MANAGER_H
