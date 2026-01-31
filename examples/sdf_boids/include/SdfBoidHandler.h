#pragma once

#include "entity.h"
#include "dot.h"
#include <vector>

namespace Boidsish {

    class SdfBoid : public Entity<Dot> {
    public:
        SdfBoid(int id, bool predator = false);
        void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;

        bool IsPredator() const { return is_predator_; }
        int GetSdfSourceId() const { return sdf_source_id_; }
        void SetSdfSourceId(int id) { sdf_source_id_ = id; }

    private:
        bool is_predator_;
        int  sdf_source_id_ = -1;
    };

    class SdfBoidHandler : public EntityHandler {
    public:
        SdfBoidHandler(task_thread_pool::task_thread_pool& thread_pool, std::shared_ptr<Visualizer>& visualizer);
        virtual ~SdfBoidHandler();

    protected:
        void PostTimestep(float time, float delta_time) override;
        void OnEntityUpdated(std::shared_ptr<EntityBase> entity) override;
    };

} // namespace Boidsish
