# Graphics System Insights

## Discovered Patterns & Anti-patterns

### 1. Resource Lifecycle Management (RAII)
- **Issue Type**: Memory Leak (OpenGL Objects)
- **Location**: `src/graphics.cpp`, `VisualizerImpl::~VisualizerImpl`
- **Evidence**: `main_fbo_`, `main_fbo_texture_`, `main_fbo_depth_texture_`, `lighting_ubo`, and `visual_effects_ubo` are created in the constructor but not deleted in the destructor.
- **Learning**: Explicit management of OpenGL resources in `VisualizerImpl` is incomplete, leading to leaks when the visualizer is destroyed and recreated.

### 2. Improper Resource Management in Instancing
- **Issue Type**: Memory Leak (OpenGL Buffers)
- **Location**: `src/instance_manager.cpp`, `InstanceManager::Render`
- **Evidence**: `m_instance_groups.clear()` is called every frame. `InstanceGroup` contains raw OpenGL buffer handles (`instance_matrix_vbo_`, `instance_color_vbo_`) but has no destructor. Clearing the map leaks these buffers every frame.
- **Learning**: The instancing system was designed for convenience but failed to account for the lifecycle of the buffers it creates dynamically.

### 3. Missing Resource Cleanup in Trail System
- **Issue Type**: Memory Leak (OpenGL Buffer)
- **Location**: `src/trail.cpp`, `Trail::~Trail`
- **Evidence**: `ebo` is generated in the constructor but not deleted in the destructor.
- **Learning**: Basic RAII compliance was missed for the element buffer in the `Trail` class.

## Rationale for Fixes
- **Fix 1**: Add missing `glDelete*` calls to `VisualizerImpl` destructor to ensure all main scene resources are freed.
- **Fix 2**: Implement a destructor for `InstanceManager` (and ideally `InstanceGroup`) to clean up VBOs. Modify `InstanceManager::Render` to reuse VBOs by clearing only the shapes vector instead of the entire map.
- **Fix 3**: Add missing `glDeleteBuffers(1, &ebo)` to `Trail::~Trail`.
