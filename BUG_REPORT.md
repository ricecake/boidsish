# Bug Report and Prevention Plan

## 1. Executive Summary

This report details a critical architectural flaw in the Boidsish project related to object lifetimes and thread safety, which could lead to race conditions, instability, and performance degradation. The core issue was the ephemeral nature of renderable objects (`Shapes`), which were destroyed and recreated every frame.

This flaw has been addressed by refactoring the rendering pipeline to use a persistent scene graph and a thread-safe command queue for inter-thread communication. This document outlines the original problem, the implemented solution, and a set of best practices to prevent similar issues in the future.

## 2. The Problem: Ephemeral Shapes and Race Conditions

The original architecture had the following characteristics:

*   **`EntityHandler`:** The simulation heart of the application, responsible for updating entity logic. Its main `operator()` function would run, potentially in parallel, and at the end of each tick, it would generate a `std::vector<std::shared_ptr<Shape>>` containing all the objects to be rendered in the current frame.
*   **`Visualizer`:** The rendering engine, which would receive this vector of shapes, render them, and then discard the vector.

This design, while simple, had several critical flaws:

*   **Race Conditions:** The `EntityHandler` could be adding or removing entities in its internal data structures while the `Visualizer` was iterating over the vector of shapes from the previous frame. This could lead to a variety of concurrency-related bugs, from memory corruption to crashes.
*   **Object Lifetime Issues:** Because the `Shape` objects were destroyed and recreated every frame, it was difficult to manage state that needed to persist across frames, such as animations or GPU resource handles.
*   **Performance Overhead:** The constant creation and destruction of `shared_ptr` objects, along with the repeated memory allocation for the shape vector, introduced unnecessary performance overhead.

## 3. The Solution: A Persistent Scene and Command Queue

To address these issues, the rendering and entity management systems were refactored to use a more robust, persistent architecture:

*   **Persistent Scene Graph:** The `Visualizer` now owns and maintains a persistent `std::map<int, std::shared_ptr<Shape>>`. This map acts as a simple scene graph, where each `Shape` has a stable identity and lifecycle.
*   **Thread-Safe Command Queue:** A new `Renderer::CommandQueue` was introduced. This is a thread-safe queue that allows the `EntityHandler` (running in the simulation thread) to safely send commands to the `Visualizer` (running in the main/render thread).
*   **Command-Based Communication:** The `EntityHandler` no longer returns a vector of shapes. Instead, when an entity is created, it pushes an `AddShapeCommand` to the queue. When an entity is destroyed, it pushes a `RemoveShapeCommand`.
*   **Decoupled Update and Render:** The `Visualizer` processes the command queue at the start of each frame, updating its persistent scene graph. The rendering logic then iterates over this stable collection of shapes.

This new architecture provides several key benefits:

*   **Thread Safety:** All modifications to the scene graph are now marshaled through the command queue, ensuring that the `Visualizer`'s data structures are only ever accessed from the main thread.
*   **Stable Object Lifetimes:** `Shape` objects now persist as long as their corresponding entities, allowing for more complex and stateful rendering.
*   **Improved Performance:** The elimination of the per-frame shape vector and the reduced churn of `shared_ptr` objects reduces memory allocation overhead.

## 4. Prevention Plan and Best Practices

To prevent similar issues from arising in the future, all developers working on the Boidsish project should adhere to the following guidelines:

*   **All Scene Modifications Must Use the Command Queue:** Any code that needs to add or remove a renderable object from the scene must do so by pushing a command to the `Visualizer`'s command queue. Direct modification of the `Visualizer`'s shape collection from outside the main thread is strictly forbidden.
*   **Entity-Shape Parity:** Every `Entity` should have a corresponding `Shape` that is created when the entity is created and destroyed when the entity is destroyed. The `EntityHandler` is now responsible for managing this lifecycle.
*   **State Synchronization:** The `Entity::UpdateShape()` method remains the primary mechanism for synchronizing state (position, color, etc.) from the simulation entity to its renderable shape. This should be called at the end of each simulation tick.
*   **Non-Entity Shapes:** For renderable objects that are not tied to an entity (e.g., paths, debug visualizations), a dedicated `EntityHandler` should be created to manage their lifecycle. This ensures that their addition and removal from the scene is still handled through the command queue.

By following these guidelines, we can ensure that the Boidsish project remains stable, performant, and free of the concurrency issues that plagued the original architecture.
