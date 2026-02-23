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
- **Fix 8**: Add `glDeleteBuffers(1, &temporal_data_ubo)` to `VisualizerImpl` destructor to fix a memory leak.
- **Fix 9**: Optimized trail vertex data passing by using type-safe vector references, avoiding per-frame `std::vector` allocations and copies in the `Trail` and `TrailRenderManager` systems.
- **Fix 10**: Improved `FireEffectManager` performance by implementing capacity tracking and `glBufferSubData` for the `terrain_chunk_buffer_`, reducing driver-side reallocation overhead.
- **Fix 11**: Implemented a stale instance group cleanup mechanism in `InstanceManager` to prevent unbounded memory growth from OpenGL buffers when many unique model types are used over time.

### 7. Missing Resource Cleanup in Visualizer (Additional)
- **Issue Type**: Memory Leak (OpenGL Buffer)
- **Location**: `src/graphics.cpp`, `VisualizerImpl::~VisualizerImpl`
- **Evidence**: `temporal_data_ubo` was created in the constructor but missing from the destructor.
- **Learning**: Uniform Buffer Objects (UBOs) are just as susceptible to leaks as textures or VBOs; audits must cover all `glGenBuffers` calls.

### 8. Excessive Memory Allocations in Trail System
- **Issue Type**: Performance Anti-pattern (CPU Overhead)
- **Location**: `src/trail.cpp`, `src/trail_render_manager.cpp`
- **Evidence**: `GetInterleavedVertexData` returned a new `std::vector<float>` every time a trail was dirty, causing frequent heap allocations.
- **Learning**: Direct access to existing buffer data (e.g., via `const std::vector<T>&`) is essential for high-frequency updates in rendering loops. Type safety should be maintained by sharing data structures (like `TrailVertex`) across related systems.

### 9. Suboptimal SSBO Updates in Fire System
- **Issue Type**: Performance Anti-pattern (Pipeline Jitter)
- **Location**: `src/fire_effect_manager.cpp`
- **Evidence**: `terrain_chunk_buffer_` was being reallocated via `glBufferData` every frame regardless of size changes.
- **Learning**: Reallocating GPU buffers frequently can cause driver stalls. Reusing capacity with `glBufferSubData` provides more stable frame times.

### 10. Unbounded Map Growth in Instance Manager
- **Issue Type**: Memory Leak (Map/VBO Bloat)
- **Location**: `src/instance_manager.cpp`
- **Evidence**: `m_instance_groups` map grew indefinitely as new instance keys (e.g., model paths) were added, never cleaning up VBOs for models no longer in use.
- **Learning**: Resource managers that use keys to cache GPU objects must implement eviction policies (like LRU or TTL) to remain deterministic over long sessions.
