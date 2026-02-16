# Graphics System Insights

## Discovered Patterns & Anti-patterns

### 1. Resource Lifecycle Management (RAII)
- **Issue Type**: Memory Leak (OpenGL Objects)
- **Location**: `src/graphics.cpp`, `VisualizerImpl::~VisualizerImpl`
- **Evidence**: `main_fbo_`, `main_fbo_texture_`, `main_fbo_depth_texture_`, `lighting_ubo`, and `visual_effects_ubo` are created in the constructor but were initially missing from the destructor.
- **Learning**: Explicit management of OpenGL resources in `VisualizerImpl` must be exhaustive to prevent driver-side leaks during visualizer recreation.

### 2. Improper Resource Management in Instancing
- **Issue Type**: Memory Leak (OpenGL Buffers)
- **Location**: `src/instance_manager.cpp`, `InstanceManager::Render`
- **Evidence**: `m_instance_groups.clear()` was being called every frame without deleting the associated VBOs.
- **Learning**: Instancing groups must own their buffer lifetimes. Reusing buffers across frames is preferred over frequent creation/deletion.

### 3. Missing Resource Cleanup in Trail System
- **Issue Type**: Memory Leak (OpenGL Buffer)
- **Location**: `src/trail.cpp`, `Trail::~Trail`
- **Evidence**: `ebo` was generated in the constructor but not deleted in the destructor.
- **Learning**: Basic RAII compliance was missed for the element buffer in the `Trail` class.

### 4. Missing Resource Cleanup in Shader System
- **Issue Type**: Memory Leak (OpenGL Programs)
- **Location**: `external/include/shader.h`, `ShaderBase`
- **Evidence**: `ShaderBase` and its subclasses lacked a destructor to call `glDeleteProgram(ID)`. Shader programs remained in GPU memory after the C++ objects were destroyed.
- **Learning**: RAII must be consistently applied to all OpenGL object wrappers to prevent driver-side memory exhaustion.

### 5. Redundant State Changes in Shader Uniforms
- **Issue Type**: Performance Anti-pattern (Redundant API Calls)
- **Location**: `external/include/shader.h`, `ShaderBase::set*`
- **Evidence**: Every uniform update called `glGetUniformLocation`, which involves string hashing and lookups in the driver.
- **Learning**: Caching uniform locations on the CPU side is a standard optimization that reduces the overhead of the rendering loop, especially when many uniforms are updated frequently.

### 6. Missing Resource Cleanup in Visualizer
- **Issue Type**: Memory Leak (OpenGL Buffer)
- **Location**: `src/graphics.cpp`, `VisualizerImpl::~VisualizerImpl`
- **Evidence**: `frustum_ubo` was created in the constructor but not deleted in the destructor.
- **Learning**: Centralized resource management or a more robust RAII wrapper system could mitigate missed cleanups as the codebase grows.

### 7. Variable Timestep Instability
- **Issue Type**: Performance & Stability Anti-pattern
- **Location**: `src/graphics.cpp`, `Visualizer::Update`
- **Evidence**: `simulation_time` and system updates (fire, explosion, etc.) were tied to variable frame delta time, leading to non-deterministic behavior.
- **Learning**: Implementing a fixed timestep with an accumulator decouples simulation logic from rendering framerate, ensuring consistent physics and effect behavior across different hardware.

### 8. Ever-growing Memory in Chase Targets
- **Issue Type**: Memory Leak (CPU)
- **Location**: `src/graphics.cpp`, `Visualizer::Update`
- **Evidence**: `chase_targets_` vector could grow indefinitely if many entities were added but `CycleChaseTarget` was never called.
- **Learning**: Periodic cleanup of weak pointer containers is necessary when object lifetimes are managed externally and registration is frequent.

### 9. Redundant Matrix Calculations and Reallocations
- **Issue Type**: Performance Anti-pattern
- **Location**: `src/graphics.cpp`, `Visualizer::Render`
- **Evidence**: View matrices were recalculated multiple times per frame, and the `shapes` vector was frequently reallocating.
- **Learning**: Reusing pre-calculated matrices and using `reserve()` on per-frame containers reduces CPU overhead and avoids costly reallocations in the hot loop.

## Rationale for Fixes
- **Fix 1**: Add missing `glDelete*` calls to `VisualizerImpl` destructor to ensure all main scene resources are freed.
- **Fix 2**: Implement a destructor for `InstanceManager` to clean up VBOs and reuse them across frames.
- **Fix 3**: Add missing `glDeleteBuffers(1, &ebo)` to `Trail::~Trail`.
- **Fix 4**: Add a virtual destructor to `ShaderBase` that calls `glDeleteProgram(ID)`.
- **Fix 5**: Implement uniform location caching in `ShaderBase` using a `std::unordered_map` to minimize `glGetUniformLocation` overhead.
- **Fix 6**: Disable copy operations and implement move operations for `ShaderBase` (Rule of Five) to safely manage OpenGL program ownership.
- **Fix 7**: Add `glDeleteBuffers(1, &frustum_ubo)` to `VisualizerImpl` destructor.
- **Fix 8**: Implemented a 60Hz fixed timestep simulation loop in `Visualizer::Update` and moved stateful system updates into it.
- **Fix 9**: Added periodic cleanup (every 600 frames) of expired weak pointers in `chase_targets_`.
- **Fix 10**: Optimized `Visualizer::Render` by adding `reserve()` to the shapes vector and reusing the initial view matrix for frustum UBO updates.
