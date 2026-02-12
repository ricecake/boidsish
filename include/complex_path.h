#pragma once

#include "path.h"
#include "terrain_generator.h"

// Forward declaration for Camera to avoid including graphics.h in a header
struct Camera;

namespace Boidsish {

class ComplexPath : public Path {
public:
    ComplexPath(int id, const TerrainGenerator* terrain_generator, Camera* camera);
    ~ComplexPath();

    void Update();
    void SetHeight(float height);
    void render() const override;
    void render(Shader& shader, const glm::mat4& model_matrix) const override;

private:
    const TerrainGenerator* terrain_generator_;
    Camera* camera_;
    float height_ = 0.5f;
    float path_length_ = 300.0f;
    float segment_distance_ = 4.0f;

};

} // namespace Boidsish
