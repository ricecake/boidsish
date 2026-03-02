# Performance Log

## Optimization: Bindless Textures
- **Technique**: `GL_ARB_bindless_texture` (glGetTextureHandleARB / glMakeTextureHandleResidentARB)
- **Rationale**: Bypasses the 16-32 texture unit limit and the CPU-side overhead of `glBindTexture`. In scenes with hundreds of unique materials, this eliminates a significant driver bottleneck during command recording.
- **Safe Alternative**: Texture Arrays or standard `glBindTexture` before each draw call.
- **Impact (Hypothetical)**:
    - CPU: ~15% reduction in frame time for complex scenes with many unique textures.
    - GPU: Reduced state change overhead.

## Optimization: GPU-Accelerated Frustum & Occlusion Culling with Command Compaction
- **Technique**: Compute Shader (`cull.comp`) + `glMultiDrawElementsIndirectCountARB`.
- **Rationale**: Offloads frustum and Hi-Z occlusion culling to the GPU. Instead of the vertex shader simply discarding vertices (which still costs a draw command and VS invocation), command compaction filters the `DrawElementsIndirectCommand` buffer itself. Only visible objects are ever submitted to the rasterizer.
- **Safe Alternative**: CPU-side frustum culling + Vertex-shader-only culling.
- **Impact (Hypothetical)**:
    - CPU: ~20% reduction in frame time (zero CPU-side culling logic).
    - GPU: ~30-50% reduction in vertex shader invocations in occluded/dense scenes.

## Optimization: Zero-Overhead Instrumentation
- **Technique**: Macro-gated scoped timers (`PROJECT_PROFILE_SCOPE`).
- **Rationale**: Allows for precise profiling of "Hot Paths" (Update/Render/Batching) without incurring any cost in production builds. Essential for identifying regressions.
- **Safe Alternative**: Always-on profiling or external tools like NSight/RenderDoc.
- **Impact**: Zero impact on production builds; enables data-driven optimization during development.
