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

- **Issue Type**: Build Error (Clang / Windows)
- **Location**: `include/post_processing/effects/FilmGrainEffect.h`
- **Evidence**: `invalid application of 'sizeof' to an incomplete type 'Shader'`. This occurs when a `std::unique_ptr` is used with a forward-declared class, but the destructor of the owning class is implicitly generated in a context where the type is still incomplete.
- **Learning**: Always explicitly define destructors in the `.cpp` file when using `std::unique_ptr` with forward-declared classes to ensure the full type is available for the internal `delete` call.

## Graphics Performance

- **Issue Type**: Potential Pipeline Stall / Redundant State Change
- **Location**: `src/graphics.cpp`, `VisualizerImpl::SetupShaderBindings`
- **Evidence**: `shader_to_setup.use()` is called for every shader to set up uniform block bindings. While not strictly a stall, frequent shader switches during initialization or re-configuration could be optimized.
- **Learning**: Batching shader configuration or using `glUniformBlockBinding` without `glUseProgram` (which is possible) can reduce overhead.

- **Issue Type**: CPU-Bound Frustum Culling
- **Location**: `src/graphics.cpp`, `VisualizerImpl::CalculateFrustum`
- **Evidence**: Frustum culling is performed on the CPU for every frame. As mentioned in the audit, this scales poorly with object count.
- **Learning**: Moving culling to a compute shader using SSBOs for object bounds would significantly improve CPU throughput.
