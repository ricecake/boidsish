#include "spatial_octree.h"
#include <algorithm>
#include <cmath>

namespace Boidsish {

SpatialOctree::SpatialOctree(const glm::vec3& center, float size, int max_depth)
    : center_(center), size_(size), max_depth_(max_depth) {
    root_ = std::make_unique<Node>();
}

void SpatialOctree::AddDensity(const glm::vec3& pos, float amount) {
    glm::vec3 min = center_ - glm::vec3(size_ * 0.5f);
    glm::vec3 max = center_ + glm::vec3(size_ * 0.5f);

    // Bounds check
    if (pos.x < min.x || pos.x > max.x || pos.y < min.y || pos.y > max.y || pos.z < min.z || pos.z > max.z) {
        return;
    }

    AddDensityRecursive(root_.get(), min, max, 0, pos, amount);
}

void SpatialOctree::AddDensityRecursive(Node* node, const glm::vec3& min, const glm::vec3& max, int depth, const glm::vec3& pos, float amount) {
    if (depth == max_depth_) {
        node->density += amount;
        return;
    }

    if (node->is_leaf) {
        Subdivide(node);
    }

    int index = GetChildIndex(min, max, pos);
    glm::vec3 child_min, child_max;
    GetChildBounds(min, max, index, child_min, child_max);
    AddDensityRecursive(node->children[index].get(), child_min, child_max, depth + 1, pos, amount);

    // Update internal node density as the maximum of its children.
    // This ensures that density is not "diluted" at higher levels of the tree,
    // which allows for effective pruning during traversal.
    float max_density = 0.0f;
    for (int i = 0; i < 8; ++i) {
        max_density = std::max(max_density, node->children[i]->density);
    }
    node->density = max_density;
}

float SpatialOctree::Sample(const glm::vec3& pos) const {
    glm::vec3 min = center_ - glm::vec3(size_ * 0.5f);
    glm::vec3 max = center_ + glm::vec3(size_ * 0.5f);

    if (pos.x < min.x || pos.x > max.x || pos.y < min.y || pos.y > max.y || pos.z < min.z || pos.z > max.z) {
        return 0.0f;
    }

    return SampleRecursive(root_.get(), min, max, 0, pos);
}

float SpatialOctree::SampleRecursive(Node* node, const glm::vec3& min, const glm::vec3& max, int depth, const glm::vec3& pos) const {
    if (node->is_leaf || depth == max_depth_) {
        return node->density;
    }

    int index = GetChildIndex(min, max, pos);
    glm::vec3 child_min, child_max;
    GetChildBounds(min, max, index, child_min, child_max);
    return SampleRecursive(node->children[index].get(), child_min, child_max, depth + 1, pos);
}

glm::vec3 SpatialOctree::GetGradient(const glm::vec3& pos) const {
    float eps = GetVoxelSize();
    float d = Sample(pos);
    float dx = Sample(pos + glm::vec3(eps, 0, 0)) - Sample(pos - glm::vec3(eps, 0, 0));
    float dy = Sample(pos + glm::vec3(0, eps, 0)) - Sample(pos - glm::vec3(0, eps, 0));
    float dz = Sample(pos + glm::vec3(0, 0, eps)) - Sample(pos - glm::vec3(0, 0, eps));

    return glm::vec3(dx, dy, dz) / (2.0f * eps);
}

void SpatialOctree::Update(float dt, float diffusion_rate, float decay_rate, const glm::vec3& drift) {
    // 1. Apply Drift by shifting the center
    // This is an O(1) approximation of uniform drift.
    center_ += drift * dt;

    // 2. Apply Diffusion and Decay
    ApplyDiffusionAndDecay(dt, diffusion_rate, decay_rate);
}

void SpatialOctree::ApplyDiffusionAndDecay(float dt, float diffusion, float decay) {
    if (diffusion <= 0.0f && decay <= 0.0f) return;

    // Collect current state for diffusion
    struct Leaf {
        glm::vec3 pos;
        float density;
    };
    std::vector<Leaf> leaves;
    if (diffusion > 0.0f) {
        Traverse([&](const glm::vec3& min, const glm::vec3& max, float d) {
            leaves.push_back({(min + max) * 0.5f, d});
        });
    }

    // Apply Decay in-place
    std::function<void(Node*)> decay_node = [&](Node* n) {
        n->density *= std::max(0.0f, 1.0f - decay * dt);
        if (!n->is_leaf) {
            for (int i = 0; i < 8; ++i) {
                decay_node(n->children[i].get());
            }
        }
    };
    decay_node(root_.get());

    // Apply Diffusion by redistributing collected density
    if (diffusion > 0.0f) {
        float voxel_size = GetVoxelSize();
        glm::vec3 dirs[6] = {
            {voxel_size, 0, 0},  {-voxel_size, 0, 0},
            {0, voxel_size, 0},  {0, -voxel_size, 0},
            {0, 0, voxel_size},  {0, 0, -voxel_size}
        };

        for (const auto& leaf : leaves) {
            float flux = leaf.density * diffusion * dt;
            if (flux < 0.0001f) continue;

            for (int i = 0; i < 6; ++i) {
                AddDensity(leaf.pos + dirs[i], flux / 6.0f);
            }
            // To maintain mass, we should subtract the total flux from the original cell.
            // However, in a simple "see it drift" scent model, often we don't strictly maintain mass
            // to allow for easier visualization. But let's do it for better behavior.
            AddDensity(leaf.pos, -flux);
        }
    }
}

void SpatialOctree::Traverse(std::function<void(const glm::vec3& min, const glm::vec3& max, float density)> callback) const {
    glm::vec3 min = center_ - glm::vec3(size_ * 0.5f);
    glm::vec3 max = center_ + glm::vec3(size_ * 0.5f);
    TraverseRecursive(root_.get(), min, max, 0, callback);
}

void SpatialOctree::TraverseRecursive(Node* node, const glm::vec3& min, const glm::vec3& max, int depth, std::function<void(const glm::vec3&, const glm::vec3&, float)> callback) const {
    if (node->density <= 0.0001f) return;

    if (node->is_leaf || depth == max_depth_) {
        callback(min, max, node->density);
        return;
    }

    for (int i = 0; i < 8; ++i) {
        if (node->children[i]) {
            glm::vec3 child_min, child_max;
            GetChildBounds(min, max, i, child_min, child_max);
            TraverseRecursive(node->children[i].get(), child_min, child_max, depth + 1, callback);
        }
    }
}

void SpatialOctree::Subdivide(Node* node) {
    node->is_leaf = false;
    for (int i = 0; i < 8; ++i) {
        node->children[i] = std::make_unique<Node>();
        // Initialize children with 0 density.
        // This avoids sudden mass surges when a node is first subdivided.
        node->children[i]->density = 0.0f;
    }
}

int SpatialOctree::GetChildIndex(const glm::vec3& min, const glm::vec3& max, const glm::vec3& pos) const {
    glm::vec3 mid = (min + max) * 0.5f;
    int index = 0;
    if (pos.x >= mid.x) index |= 1;
    if (pos.y >= mid.y) index |= 2;
    if (pos.z >= mid.z) index |= 4;
    return index;
}

void SpatialOctree::GetChildBounds(const glm::vec3& min, const glm::vec3& max, int index, glm::vec3& child_min, glm::vec3& child_max) const {
    glm::vec3 mid = (min + max) * 0.5f;
    child_min = min;
    child_max = max;

    if (index & 1) child_min.x = mid.x; else child_max.x = mid.x;
    if (index & 2) child_min.y = mid.y; else child_max.y = mid.y;
    if (index & 4) child_min.z = mid.z; else child_max.z = mid.z;
}

float SpatialOctree::GetVoxelSize() const {
    return size_ / std::pow(2.0f, max_depth_);
}

} // namespace Boidsish
