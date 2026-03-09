# Performance Analysis Log

## Optimizations Summary

### 1. Bindless Textures (`GL_ARB_bindless_texture`)
- **Technique**: Treat textures as 64-bit resident handles instead of binding to texture units.
- **Hypothetical Impact**:
    - **Before**: Every `RenderPacket` with unique textures triggered multiple `glBindTexture` calls, forcing a "batch break" in Multi-Draw Indirect (MDI) logic. Maximum batch size was limited by the number of objects sharing the exact same texture set.
    - **After**: All texture handles are passed via the `CommonUniforms` SSBO. `ExecuteRenderQueue` no longer calls `glBindTexture` for bindless-capable shaders.
    - **Expected Result**: 10-20% reduction in CPU driver overhead and significantly fewer draw calls via improved MDI batching (often reducing 100+ draw calls to <5).

### 2. Multi-Draw Indirect (MDI) Batching Refinement
- **Technique**: Updated `can_batch` logic to ignore texture differences when bindless is active.
- **Hypothetical Impact**:
    - **Before**: Batching was highly fragmented due to texture state changes.
    - **After**: Batching is now primarily limited by VAO and Shader state, allowing for massive consolidation of draw commands.

### 3. Zero-Overhead Instrumentation
- **Technique**: Macro-gated `PROJECT_PROFILE_SCOPE` and `PROJECT_MARKER` using `glPushDebugGroup` and `std::chrono`.
- **Impact**: Provides precision profiling in development with zero runtime cost in production builds (`PROFILING_ENABLED` gated).

## Alignment and Safety
- **SSBO Alignment**: `CommonUniforms` was increased to 512 bytes with explicit padding to ensure `std430` alignment safety and optimal GPU cache line utilization.
- **Fallback Path**: Maintained a standard `glBindTexture` path for hardware that does not support `GL_ARB_bindless_texture`.

## Metrics (Hypothetical)
| Scenario | Draw Calls (Before) | Draw Calls (After) | CPU Frame Time (est) |
|---|---|---|---|
| Complex Scene (1000+ objects) | ~250 | ~3 | -1.5ms |
| Shadow Pass (4 cascades) | ~40 | ~4 | -0.8ms |
