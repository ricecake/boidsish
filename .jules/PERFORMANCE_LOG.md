# Performance Log

## Persistent Mapped UBOs (AZDO)
- **Before**: Core UBOs (Lighting, TemporalData, VisualEffects) updated via `glBufferSubData` every frame. This causes implicit driver synchronization and stalls if the GPU is still reading from the buffer.
- **After**: Implemented triple-buffered `PersistentBuffer<T>`. CPU writes directly to mapped memory, and `glBindBufferRange` is used to select the correct segment. This eliminates `glBufferSubData` overhead and synchronization stalls.
- **Why**: Standard AZDO technique for high-frequency data.

## Unified GPU Visibility Pass
- **Before**: Frustum culling performed per-vertex in `vis.vert` (outputting degenerate triangles) or partially on CPU. Occlusion culling performed in a separate compute pass (`occlusion_cull.comp`).
- **After**: Integrated frustum culling into `occlusion_cull.comp`. A single compute pass now determines both frustum and Hi-Z visibility, writing to a visibility SSBO.
- **Why**: Reduces compute dispatch overhead and centralizes visibility logic. Leverages the existing MDI infrastructure to skip occluded/off-screen objects entirely.

## Render Queue State Caching
- **Before**: `ExecuteRenderQueue` would re-bind textures and toggle culling state for every batch, even if the state was identical to the previous batch.
- **After**: Implemented local state caching for texture IDs and `GL_CULL_FACE`. Driver calls are only made when the state actually changes.
- **Why**: Minimizes driver validation overhead and state transition costs.

## Zero-Overhead Instrumentation
- **Added**: `PROJECT_PROFILE_SCOPE` markers to all primary rendering functions in `src/graphics.cpp`.
- **Impact**: Provides high-resolution profiling data without any runtime cost in production builds.
