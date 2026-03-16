# Performance Log

## Bindless Textures (AZDO)
- **Technique**: Replaced traditional `glBindTexture` / `glActiveTexture` with `GL_ARB_bindless_texture` handles in the `CommonUniforms` SSBO.
- **Impact**: Significant reduction in driver overhead and CPU-GPU stalls by eliminating texture unit binding state changes. Enables more efficient Multi-Draw Indirect (MDI) batches where textures previously forced batch breaks.
- **Fallback**: Implemented a check for `BindlessTextureManager::GetInstance().IsSupported()`. If unsupported, the system automatically falls back to legacy texture binding logic in `ExecuteRenderQueue`.

## Zero-Overhead Instrumentation
- **Technique**: Macro-gated `PROJECT_PROFILE_SCOPE` and `PROJECT_MARKER` macros that resolve to `Boidsish::ProfileScope` when `PROFILING_ENABLED` is defined.
- **Impact**: Allows precision profiling of hot paths (`Visualizer::Render`, `ExecuteRenderQueue`, `UpdateTrails`, `RenderTerrain`) with zero runtime overhead in production builds.

## Synchronous Call Removal
- **Technique**: Removed `glCheckFramebufferStatus` from `ShadowManager::BeginShadowPass` hot path. Cached uniform locations for `occlusion_cull.comp` to avoid per-frame `glGetUniformLocation` calls.
- **Impact**: Eliminates pipeline stalls caused by synchronous driver-to-GPU queries during the main render loop.

## CommonUniforms Optimization
- **Technique**: Reorganized `CommonUniforms` to include 64-bit texture handles while maintaining exact 256-byte alignment (std430) for optimal SSBO performance and cache locality.
- **Impact**: Ensures data locality and avoids performance penalties associated with misaligned SSBO access.
