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

class VectorFieldHandler;

// An entity that is affected by a vector field
class FieldEntity : public Entity {
public:
    FieldEntity(int id) : Entity(id) {}

    virtual void UpdateEntity(EntityHandler& handler, float time, float delta_time) override = 0;
};

class VectorFieldHandler : public EntityHandler {
public:
    VectorFieldHandler(int field_width, int field_height, int field_depth)
        : field_width_(field_width), field_height_(field_height), field_depth_(field_depth), current_field_(0) {}

    void CreateField(const std::string& name) {
        fields_[name][0] = std::make_unique<VectorField>(field_width_, field_height_, field_depth_);
        fields_[name][1] = std::make_unique<VectorField>(field_width_, field_height_, field_depth_);
    }

    void AddEmitter(std::shared_ptr<FieldEmitter> emitter) {
        emitters_.push_back(emitter);
    }

    Vector3 GetFieldSumAt(const Vector3& position) const {
        Vector3 sum = Vector3::Zero();
        for (const auto& emitter : emitters_) {
            AABB box = emitter->GetBoundingBox();
            if (position.x >= box.min.x && position.x < box.max.x &&
                position.y >= box.min.y && position.y < box.max.y &&
                position.z >= box.min.z && position.z < box.max.z) {
                sum += emitter->GetFieldContribution(position);
            }
        }
        return sum;
    }

    void AddToPersistentField(const std::string& name, const Vector3& position, const Vector3& value) {
        int x = static_cast<int>(position.x);
        int y = static_cast<int>(position.y);
        int z = static_cast<int>(position.z);
        fields_.at(name)[1 - current_field_]->AddValue(x, y, z, value);
    }

    const VectorField& GetPersistentField(const std::string& name) const {
        return *fields_.at(name)[current_field_];
    }

protected:
    void PreTimestep(float time, float delta_time) override;
    void PostTimestep(float time, float delta_time) override;

private:
    void SwapFields() {
        current_field_ = 1 - current_field_;
    }

    int field_width_, field_height_, field_depth_;
    std::map<std::string, std::unique_ptr<VectorField>[2]> fields_;
    int current_field_;
    std::vector<std::shared_ptr<FieldEmitter>> emitters_;
};

}