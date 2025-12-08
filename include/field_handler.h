#pragma once

#include "boidsish.h"
#include <vector>
#include <memory>

namespace Boidsish {

// Represents a 3D vector field
class VectorField {
public:
    VectorField(int width, int height, int depth)
        : width_(width), height_(height), depth_(depth) {
        field_.resize(width * height * depth, Vector3::Zero());
    }

    Vector3 GetValue(int x, int y, int z) const {
        if (x < 0 || x >= width_ || y < 0 || y >= height_ || z < 0 || z >= depth_) {
            return Vector3::Zero();
        }
        return field_[z * width_ * height_ + y * width_ + x];
    }

    void SetValue(int x, int y, int z, const Vector3& value) {
        if (x >= 0 && x < width_ && y >= 0 && y < height_ && z >= 0 && z < depth_) {
            field_[z * width_ * height_ + y * width_ + x] = value;
        }
    }

    void AddValue(int x, int y, int z, const Vector3& value) {
        if (x >= 0 && x < width_ && y >= 0 && y < height_ && z >= 0 && z < depth_) {
            field_[z * width_ * height_ + y * width_ + x] += value;
        }
    }

    void Clear() {
        std::fill(field_.begin(), field_.end(), Vector3::Zero());
    }

    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }
    int GetDepth() const { return depth_; }

private:
    int width_, height_, depth_;
    std::vector<Vector3> field_;
};

// Interface for an object that emits a field
struct AABB {
    Vector3 min;
    Vector3 max;
};

class FieldEmitter {
public:
    virtual ~FieldEmitter() = default;
    virtual Vector3 GetFieldContribution(const Vector3& position) const = 0;
    virtual AABB GetBoundingBox() const = 0;
};

// An entity that is affected by a vector field
class FieldEntity : public Entity {
public:
    FieldEntity(int id) : Entity(id) {}

    void UpdateEntity(EntityHandler& handler, float time, float delta_time) override {
        // Specific update logic for field entities will be in the handler
        (void)handler;
        (void)time;
        (void)delta_time;
    }
};

class VectorFieldHandler : public EntityHandler {
public:
    VectorFieldHandler(int field_width, int field_height, int field_depth)
        : current_field_(0) {
        fields_[0] = std::make_unique<VectorField>(field_width, field_height, field_depth);
        fields_[1] = std::make_unique<VectorField>(field_width, field_height, field_depth);
    }

    void AddEmitter(std::shared_ptr<FieldEmitter> emitter) {
        emitters_.push_back(emitter);
    }

    const VectorField& GetCurrentField() const {
        return *fields_[current_field_];
    }

protected:
    void PreTimestep(float time, float delta_time) override;
    void PostTimestep(float time, float delta_time) override;

private:
    void SwapFields() {
        current_field_ = 1 - current_field_;
    }

    std::unique_ptr<VectorField> fields_[2];
    int current_field_;
    std::vector<std::shared_ptr<FieldEmitter>> emitters_;
};

}