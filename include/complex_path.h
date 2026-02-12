#pragma once

#include "path.h"
#include "terrain_generator.h"

// Forward declaration for Camera to avoid including graphics.h in a header
struct Camera;

namespace Boidsish {

class EntityBase;

class ComplexPath : public Path {
public:
    ComplexPath(int id, const TerrainGenerator* terrain_generator, Camera* camera);
    ~ComplexPath();

    void Update();
    void SetHeight(float height) { height_ = height; }
    void SetTarget(std::shared_ptr<EntityBase> target) { target_ = target; }
    void SetMaxCurvature(float curvature) { max_curvature_ = curvature; }
    void SetRoughnessAvoidance(float avoidance) { roughness_avoidance_ = avoidance; }
    void SetPathLength(float length) { path_length_ = length; }

    void render() const override;
    void render(Shader& shader, const glm::mat4& model_matrix) const override;

    bool CastsShadows() const override { return false; }

private:
    const TerrainGenerator* terrain_generator_;
    Camera* camera_;
    std::shared_ptr<EntityBase> target_;

    float height_ = 2.0f;
    float path_length_ = 300.0f;
    float segment_distance_ = 4.0f;
    float max_curvature_ = 1.0f;
    float roughness_avoidance_ = 0.0f;

};

} // namespace Boidsish
