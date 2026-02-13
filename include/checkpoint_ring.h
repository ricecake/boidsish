#ifndef CHECKPOINT_RING_H
#define CHECKPOINT_RING_H

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "entity.h"
#include "shape.h"

namespace Boidsish {

	enum class CheckpointStyle {
		GOLD = 0,
		SILVER = 1,
		BLACK = 2,
		BLUE = 3,
		NEON_GREEN = 4,
		RAINBOW = 5,
		INVISIBLE = 6
	};

	class CheckpointRingShape: public Shape {
	public:
		CheckpointRingShape(float radius, CheckpointStyle style);

		void      render() const override;
		void      render(Shader& shader, const glm::mat4& model_matrix) const override;
		glm::mat4 GetModelMatrix() const override;

		std::string GetInstanceKey() const override { return "CheckpointRing"; }

		bool IsTransparent() const override { return true; }

		bool CastsShadows() const override { return false; }

		void SetRadius(float radius) { radius_ = radius; }

		float GetRadius() const { return radius_; }

		void SetStyle(CheckpointStyle style) { style_ = style; }

		CheckpointStyle GetStyle() const { return style_; }

		static void InitQuadMesh();
		static void DestroyQuadMesh();

		static std::shared_ptr<Shader> GetShader() { return checkpoint_shader_; }

		static void SetShader(std::shared_ptr<Shader> shader) { checkpoint_shader_ = shader; }

	private:
		float           radius_;
		CheckpointStyle style_;

		static unsigned int            quad_vao_;
		static unsigned int            quad_vbo_;
		static std::shared_ptr<Shader> checkpoint_shader_;
	};

	class CheckpointRing: public Entity<CheckpointRingShape> {
	public:
		using Callback = std::function<void(float, std::shared_ptr<EntityBase>)>;

		CheckpointRing(int id, float radius, CheckpointStyle style, Callback callback);

		void RegisterEntity(std::shared_ptr<EntityBase> entity);
		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void UpdateShape() override;

	private:
		Callback callback_;

		struct TrackedEntity {
			int                       id;
			std::weak_ptr<EntityBase> ptr;
		};

		std::vector<TrackedEntity> tracked_entities_;
		std::map<int, glm::vec3>   last_positions_;
	};

} // namespace Boidsish

#endif // CHECKPOINT_RING_H
