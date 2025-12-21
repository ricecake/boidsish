#pragma once

#include <map>
#include <memory>
#include <vector>
#include <tuple>

#include "graphics.h"
#include "point_cloud.h"
#include <glm/glm.hpp>

namespace Boidsish {

    class PointCloudVisualizer {
    public:
        PointCloudVisualizer(const std::vector<glm::vec4>& point_data);

        void update(const Frustum& frustum, const Camera& camera);
        std::vector<std::shared_ptr<PointCloud>> getVisibleChunks();
        void setThreshold(float threshold);
        float getThreshold() const;
        void setPointSize(float pointSize);
        float getPointSize() const;

    private:
        std::shared_ptr<PointCloud> generateChunk(int chunkX, int chunkY, int chunkZ);

        const int chunk_size_ = 16;
        const int view_distance_ = 10; // in chunks

        std::map<std::tuple<int, int, int>, std::vector<glm::vec4>> chunked_point_data_;
        std::map<std::tuple<int, int, int>, std::shared_ptr<PointCloud>> chunk_cache_;
        float threshold_ = 0.5f;
        float point_size_ = 5.0f;
    };

} // namespace Boidsish
