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

### 7. Missing Resource Cleanup in Visualizer (Additional)
- **Issue Type**: Memory Leak (OpenGL Buffer)
- **Location**: `src/graphics.cpp`, `VisualizerImpl::~VisualizerImpl`
- **Evidence**: `temporal_data_ubo` is created in the constructor but missing from the destructor.
- **Learning**: All UBOs must be explicitly tracked for cleanup.

### 8. Unused OpenGL Object Handle
- **Issue Type**: Code Smell / Potential Leak
- **Location**: `src/graphics.cpp`, `VisualizerImpl`
- **Evidence**: `frustum_ubo` is declared and initialized to 0, but never generated or used. Modern code uses `frustum_ssbo`.
- **Learning**: Legacy handles should be pruned to avoid confusion and accidental misuse.

### 9. Missing RAII in Mesh Class
- **Issue Type**: Memory Leak (OpenGL Objects)
- **Location**: `src/model.cpp`, `Mesh`
- **Evidence**: `Mesh` generates VAO, VBO, and EBO in `setupMesh` but lacks a destructor to call `Cleanup()`. Since `AssetManager` caches `ModelData` which contains `Mesh` objects, these are leaked if `AssetManager::Clear()` is not called or if meshes are created outside the manager.
- **Learning**: Core rendering primitives must follow RAII to ensure deterministic resource release.

### 10. Memory Leak in Embedded Textures
- **Issue Type**: Memory Leak (OpenGL Textures)
- **Location**: `src/asset_manager.cpp`, `LoadMaterialTextures`
- **Evidence**: Embedded textures are generated using `glGenTextures` but not registered with `AssetManager`'s texture cache. They are never deleted.
- **Learning**: All dynamically created OpenGL resources must be registered with a central tracking system or owned by RAII objects.

### 11. Pipeline Stall in Mesh Rendering
- **Issue Type**: Pipeline Stall
- **Location**: `src/model.cpp`, `Mesh::render`
- **Evidence**: Frequent calls to `glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, ...)` in the hot rendering path. Synchronous state queries force a CPU-GPU sync point.
- **Learning**: Avoid `glGet*` calls during frame execution. Use state tracking or assume state validity in production builds.
