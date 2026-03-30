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

## Rationale for Fixes
- **Fix 1**: Add missing `glDelete*` calls to `VisualizerImpl` destructor to ensure all main scene resources are freed.
- **Fix 2**: Implement a destructor for `InstanceManager` to clean up VBOs and reuse them across frames.
- **Fix 3**: Add missing `glDeleteBuffers(1, &ebo)` to `Trail::~Trail`.
- **Fix 4**: Add a virtual destructor to `ShaderBase` that calls `glDeleteProgram(ID)`.
- **Fix 5**: Implement uniform location caching in `ShaderBase` using a `std::unordered_map` to minimize `glGetUniformLocation` overhead.
- **Fix 6**: Disable copy operations and implement move operations for `ShaderBase` (Rule of Five) to safely manage OpenGL program ownership.
- **Fix 7**: Add `glDeleteBuffers(1, &frustum_ubo)` to `VisualizerImpl` destructor.

### 7. Missing Resource Cleanup in Visualizer (Part 2)
- **Issue Type**: Memory Leak (OpenGL Buffer)
- **Location**: `src/graphics.cpp`, `VisualizerImpl::~VisualizerImpl`
- **Evidence**: `temporal_data_ubo` was created in the constructor but not deleted in the destructor.
- **Learning**: All UBOs and global GPU resources must be tracked in the central lifecycle manager or explicitly handled in the destructor.

### 8. Missing RAII in Mesh Class
- **Issue Type**: Memory Leak (OpenGL Objects)
- **Location**: `include/model.h`, `src/model.cpp`
- **Evidence**: The `Mesh` class managed several OpenGL objects (VAO, VBO, EBO) but lacked a destructor to release them, leading to leaks when `Mesh` objects were destroyed.
- **Learning**: Core resource-holding classes must implement the Rule of Five or use smart RAII wrappers to ensure GPU memory is reclaimed.

### 9. Unbounded Container Growth (Heuristic)
- **Issue Type**: Memory Growth (CPU Memory)
- **Location**: `src/graphics.cpp`, `Visualizer::Update`
- **Evidence**: `chase_targets_` grew every time a target was added but was only sporadically cleaned up.
- **Learning**: Long-lived containers that accumulate handles must have predictable pruning logic to maintain a stable memory footprint.

## Rationale for Fixes (Updated)
- **Fix 8**: Add `glDeleteBuffers(1, &temporal_data_ubo)` to `VisualizerImpl` destructor.
- **Fix 9**: Implement `Mesh` destructor and update `Cleanup()` to safely release owned OpenGL resources.
- **Fix 10**: Implement proactive pruning of `chase_targets_` in the main update loop.
