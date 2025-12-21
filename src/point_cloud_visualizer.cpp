#include "point_cloud_visualizer.h"

#include <cmath>
#include <numeric>
#include <vector>

#include "logger.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {

    bool isChunkInFrustum3D(const Frustum& frustum, int chunkX, int chunkY, int chunkZ, int chunkSize) {
        glm::vec3 center(
            chunkX * chunkSize + chunkSize / 2.0f,
            chunkY * chunkSize + chunkSize / 2.0f,
            chunkZ * chunkSize + chunkSize / 2.0f
        );
        glm::vec3 halfSize(chunkSize / 2.0f, chunkSize / 2.0f, chunkSize / 2.0f);

        for (int i = 0; i < 6; ++i) {
            float r = halfSize.x * std::abs(frustum.planes[i].normal.x) +
                halfSize.y * std::abs(frustum.planes[i].normal.y) + halfSize.z * std::abs(frustum.planes[i].normal.z);
            float d = glm::dot(center, frustum.planes[i].normal) + frustum.planes[i].distance;
            if (d < -r) {
                return false;
            }
        }
        return true;
    }

    PointCloudVisualizer::PointCloudVisualizer(const std::vector<glm::vec4>& point_data) {
        for (const auto& point : point_data) {
            int chunkX = static_cast<int>(floor(point.x / chunk_size_));
            int chunkY = static_cast<int>(floor(point.y / chunk_size_));
            int chunkZ = static_cast<int>(floor(point.z / chunk_size_));
            chunked_point_data_[{chunkX, chunkY, chunkZ}].push_back(point);
        }
    }

    void PointCloudVisualizer::update(const Frustum& frustum, const Camera& camera) {
        int current_chunk_x = static_cast<int>(camera.x) / chunk_size_;
        int current_chunk_y = static_cast<int>(camera.y) / chunk_size_;
        int current_chunk_z = static_cast<int>(camera.z) / chunk_size_;

        for (int x = current_chunk_x - view_distance_; x <= current_chunk_x + view_distance_; ++x) {
            for (int y = current_chunk_y - view_distance_; y <= current_chunk_y + view_distance_; ++y) {
                for (int z = current_chunk_z - view_distance_; z <= current_chunk_z + view_distance_; ++z) {
                    if (isChunkInFrustum3D(frustum, x, y, z, chunk_size_)) {
                        std::tuple<int, int, int> chunk_coord = { x, y, z };
                        if (chunk_cache_.find(chunk_coord) == chunk_cache_.end()) {
                            chunk_cache_[chunk_coord] = generateChunk(x, y, z);
                        }
                    }
                }
            }
        }

        std::vector<std::tuple<int, int, int>> to_remove;
        for (auto const& [key, val] : chunk_cache_) {
            int dx = std::get<0>(key) - current_chunk_x;
            int dy = std::get<1>(key) - current_chunk_y;
            int dz = std::get<2>(key) - current_chunk_z;
            if (std::abs(dx) > view_distance_ || std::abs(dy) > view_distance_ || std::abs(dz) > view_distance_) {
                to_remove.push_back(key);
            }
        }

        for (const auto& key : to_remove) {
            chunk_cache_.erase(key);
        }
    }

    std::vector<std::shared_ptr<PointCloud>> PointCloudVisualizer::getVisibleChunks() {
        std::vector<std::shared_ptr<PointCloud>> visible_chunks;
        for (auto const& [key, val] : chunk_cache_) {
            if (val) {
                visible_chunks.push_back(val);
            }
        }
        return visible_chunks;
    }

    void PointCloudVisualizer::setThreshold(float threshold) {
		threshold_ = threshold;
	}

    float PointCloudVisualizer::getThreshold() const {
        return threshold_;
    }

    void PointCloudVisualizer::setPointSize(float pointSize) {
        point_size_ = pointSize;
    }

    float PointCloudVisualizer::getPointSize() const {
        return point_size_;
    }

    std::shared_ptr<PointCloud> PointCloudVisualizer::generateChunk(int chunkX, int chunkY, int chunkZ) {
        std::tuple<int, int, int> chunk_coord = { chunkX, chunkY, chunkZ };
        if (chunked_point_data_.find(chunk_coord) == chunked_point_data_.end()) {
            return nullptr;
        }

        const auto& chunk_points = chunked_point_data_.at(chunk_coord);
        std::vector<float> vertexData;
        vertexData.reserve(chunk_points.size() * 4);

        int startX = chunkX * chunk_size_;
        int startY = chunkY * chunk_size_;
        int startZ = chunkZ * chunk_size_;

        for (const auto& point : chunk_points) {
            vertexData.push_back(point.x - startX);
            vertexData.push_back(point.y - startY);
            vertexData.push_back(point.z - startZ);
            vertexData.push_back(point.w);
        }

        auto point_cloud_chunk = std::make_shared<PointCloud>(vertexData);
        point_cloud_chunk->SetPosition(startX, startY, startZ);

        return point_cloud_chunk;
    }

} // namespace Boidsish
