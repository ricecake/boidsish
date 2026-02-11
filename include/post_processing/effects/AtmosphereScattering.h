#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

class Shader;

namespace Boidsish {
	namespace PostProcessing {

		class AtmosphereScattering {
		public:
			struct Parameters {
				glm::vec3 rayleigh_scattering = glm::vec3(0.000464f, 0.001085f, 0.002648f);
				float     rayleigh_multiplier = 1.0f;
				float     rayleigh_scale_height = 100.0f;
				float     mie_scattering = 0.000399f;
				float     mie_multiplier = 1.0f;
				float     mie_extinction = 0.000444f;
				float     mie_anisotropy = 0.80f;
				float     mie_scale_height = 15.0f;
				glm::vec3 absorption_extinction = glm::vec3(0.000052f, 0.000150f, 0.000007f);
				float     bottom_radius = 79500.0f;
				float     top_radius = 80750.0f;
				glm::vec3 ground_albedo = glm::vec3(0.3f);
				float     sun_intensity = 30.0f;
				float     sun_intensity_factor = 15.0f;
				float     world_scale = 1.0f;

				bool operator==(const Parameters& other) const {
					return rayleigh_scattering == other.rayleigh_scattering &&
						rayleigh_multiplier == other.rayleigh_multiplier &&
						rayleigh_scale_height == other.rayleigh_scale_height &&
						mie_scattering == other.mie_scattering && mie_multiplier == other.mie_multiplier &&
						mie_extinction == other.mie_extinction && mie_anisotropy == other.mie_anisotropy &&
						mie_scale_height == other.mie_scale_height &&
						absorption_extinction == other.absorption_extinction && bottom_radius == other.bottom_radius &&
						top_radius == other.top_radius && ground_albedo == other.ground_albedo &&
						sun_intensity == other.sun_intensity && sun_intensity_factor == other.sun_intensity_factor &&
						world_scale == other.world_scale;
				}

				bool operator!=(const Parameters& other) const { return !(*this == other); }
			};

			AtmosphereScattering();
			~AtmosphereScattering();

			void Initialize();
			void Update(const Parameters& params);

			GLuint GetTransmittanceLUT() const { return transmittance_lut_; }

			GLuint GetMultiScatteringLUT() const { return multi_scattering_lut_; }

			const Parameters& GetParameters() const { return params_; }

		private:
			void RebuildLUTs();
			void GenerateTransmittanceLUT();
			void GenerateMultiScatteringLUT();

			Parameters params_;
			GLuint     transmittance_lut_ = 0;
			GLuint     multi_scattering_lut_ = 0;

			std::unique_ptr<Shader> transmittance_shader_;
			std::unique_ptr<Shader> multi_scattering_shader_;

			GLuint quad_vao_ = 0;
			GLuint quad_vbo_ = 0;

			const int transmittance_width_ = 256;
			const int transmittance_height_ = 64;
			const int multi_scattering_size_ = 32;
		};

	} // namespace PostProcessing
} // namespace Boidsish
