#pragma once

#include <vector>

#include "sdf_shape.h"
#include "entity.h"

namespace Boidsish {

	class SdfBoid: public Entity<SdfShape> {
	public:
		SdfBoid(int id, bool predator = false);
		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;

		bool IsPredator() const { return is_predator_; }

	private:
		bool is_predator_;
	};

	class SdfBoidHandler: public EntityHandler {
	public:
		SdfBoidHandler(task_thread_pool::task_thread_pool& thread_pool, std::shared_ptr<Visualizer>& visualizer);
		virtual ~SdfBoidHandler();

	protected:
		void PostTimestep(float time, float delta_time) override;
		void OnEntityUpdated(std::shared_ptr<EntityBase> entity) override;
	};

} // namespace Boidsish
