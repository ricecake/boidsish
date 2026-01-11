#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <variant>

#include "shape.h"

namespace Boidsish {
namespace Renderer {

    // Command to add a shape to the renderer
    struct AddShapeCommand {
        int id;
        std::shared_ptr<Shape> shape;
    };

    // Command to remove a shape from the renderer
    struct RemoveShapeCommand {
        int id;
    };

    // A variant type that can hold any of the renderer commands
    using Command = std::variant<AddShapeCommand, RemoveShapeCommand>;

    // A thread-safe queue for renderer commands
    class CommandQueue {
    public:
        void Push(Command command) {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(command));
        }

        bool Pop(Command& command) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.empty()) {
                return false;
            }
            command = std::move(queue_.front());
            queue_.pop();
            return true;
        }

    private:
        std::queue<Command> queue_;
        std::mutex mutex_;
    };

} // namespace Renderer
} // namespace Boidsish
