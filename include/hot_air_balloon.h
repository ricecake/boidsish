#pragma once

#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "entity.h"
#include "delaunay_blob.h"
#include "polyhedron.h"
#include "line.h"

namespace Boidsish {

	class HotAirBalloonEntity : public EntityBase {
	public:
		HotAirBalloonEntity(
			int id,
			float x,
			float y,
			float z,
			float size = 6.0f,
			const glm::vec4& color = glm::vec4(0.9f, 0.4f, 0.2f, 1.0f),
			float buoyancy = 12.0f,
			float wind_response = 1.0f
		);

		~HotAirBalloonEntity() override;

		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		std::shared_ptr<Shape> GetShape() const override;
		void UpdateShape() override;

		// Setters / Getters for settings
		void SetBuoyancy(float b) { buoyancy_ = b; }
		float GetBuoyancy() const { return buoyancy_; }

		void SetWindResponse(float w) { wind_response_ = w; }
		float GetWindResponse() const { return wind_response_; }

		void SetSpringStiffness(float s) { spring_stiffness_ = s; InitializePhysics(); }
		float GetSpringStiffness() const { return spring_stiffness_; }

		void SetSpringDamping(float d) { spring_damping_ = d; }
		float GetSpringDamping() const { return spring_damping_; }

		void SetBalloonSize(float s) { size_ = s; InitializePhysics(); }
		float GetBalloonSize() const { return size_; }

		void SetBalloonColor(const glm::vec4& c) { color_ = c; InitializePhysics(); }
		glm::vec4 GetBalloonColor() const { return color_; }

		void SetName(const std::string& name) { name_ = name; }
		const std::string& GetName() const { return name_; }

	private:
		std::shared_ptr<DelaunayBlob> shape_;
		std::shared_ptr<Polyhedron> basket_shape_;
		std::vector<std::shared_ptr<Line>> line_shapes_;
		bool basket_added_ = false;
		Visualizer* shape_handler_visualizer_pointer_ = nullptr;
		std::string name_;

		// Simulation settings
		float buoyancy_;
		float wind_response_;
		float spring_stiffness_ = 30.0f;
		float spring_damping_ = 2.0f;
		glm::vec4 color_;

		// Internal soft-body simulation structures
		struct SoftPoint {
			int id; // ID in DelaunayBlob (if envelope)
			glm::vec3 position;
			glm::vec3 velocity{0.0f};
			glm::vec3 force{0.0f};
			glm::vec3 rest_offset; // Relative to initial center
			float mass = 1.0f;
			bool is_basket = false;
			glm::vec4 color;
		};

		struct SoftSpring {
			int index_a;
			int index_b;
			float rest_length;
			float stiffness;
		};

		std::vector<SoftPoint> points_;
		std::vector<SoftSpring> springs_;

		void InitializePhysics();
	};

} // namespace Boidsish
