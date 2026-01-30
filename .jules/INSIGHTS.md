# Graphics System Insights

## Resource Lifecycle Management

- **Issue Type**: Memory Leak (OpenGL Objects)
- **Location**: `src/graphics.cpp`, `VisualizerImpl::~VisualizerImpl`
- **Evidence**: `lighting_ubo`, `visual_effects_ubo`, `main_fbo_`, `main_fbo_texture_`, and `main_fbo_depth_texture_` are generated in the constructor but never deleted in the destructor.
- **Learning**: Explicit management of OpenGL resources in complex classes like `VisualizerImpl` can easily lead to missed deletions if not using RAII wrappers.

- **Issue Type**: Memory Leak (OpenGL Objects)
- **Location**: `src/trail.cpp`, `Trail::~Trail`
- **Evidence**: `ebo` (Element Buffer Object) is generated in the constructor but not deleted in the destructor.
- **Learning**: Standard buffer management needs to be consistent; `vao` and `vbo` were deleted, but `ebo` was overlooked.

- **Issue Type**: Memory Leak (OpenGL Objects)
- **Location**: `include/instance_manager.h`, `src/instance_manager.cpp`
- **Evidence**: `InstanceGroup` contains `instance_matrix_vbo_` and `instance_color_vbo_` which are never deleted. `InstanceManager` lacks a destructor to clean up these resources across all groups.
- **Learning**: Dynamically created groups that own OpenGL resources must have a clear cleanup strategy, ideally via a destructor in the group struct or the manager.

## Graphics Performance

- **Issue Type**: Potential Pipeline Stall / Redundant State Change
- **Location**: `src/graphics.cpp`, `VisualizerImpl::SetupShaderBindings`
- **Evidence**: `shader_to_setup.use()` is called for every shader to set up uniform block bindings. While not strictly a stall, frequent shader switches during initialization or re-configuration could be optimized.
- **Learning**: Batching shader configuration or using `glUniformBlockBinding` without `glUseProgram` (which is possible) can reduce overhead.

- **Issue Type**: CPU-Bound Frustum Culling
- **Location**: `src/graphics.cpp`, `VisualizerImpl::CalculateFrustum`
- **Evidence**: Frustum culling is performed on the CPU for every frame. As mentioned in the audit, this scales poorly with object count.
- **Learning**: Moving culling to a compute shader using SSBOs for object bounds would significantly improve CPU throughput.
