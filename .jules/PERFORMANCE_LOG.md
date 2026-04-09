# Performance Log

## GPU-Accelerated MDI Culling
**Date**: 2024-05-22
**Technique**: Move frustum and Hi-Z occlusion culling from vertex shader to a Compute shader.
**Reasoning**: The previous implementation performed culling in the vertex shader by outputting degenerate triangles. While this saved rasterization time, it still incurred vertex fetch and shader invocation costs for every vertex of a culled object. Moving this to Compute allows us to zero out `instanceCount` in the indirect buffers, effectively "deleting" the work before it hits the vertex stage.
**Hypothetical Impact**: 15-25% improvement in frame time in scenes with high object density and heavy occlusion.

## Bindless Textures (AZDO)
**Date**: 2024-05-22
**Technique**: Implement `GL_ARB_bindless_texture` support.
**Reasoning**: Texture binding was a major batch-breaking criteria in `ExecuteRenderQueue`. By using 64-bit texture handles, we can treat textures as standard UBO/SSBO data. This allows collapsing multiple batches that only differed in texture bindings into a single Multi-Draw Indirect (MDI) call.
**Hypothetical Impact**: 10-20% reduction in CPU-side driver overhead due to fewer draw calls and state changes.

## Fixed Timestep for Simulation
**Date**: 2024-05-22
**Technique**: Decouple simulation from rendering using an accumulator.
**Reasoning**: Running simulation on variable `delta_time` leads to non-deterministic behavior and instability at low framerates. A fixed 60Hz step ensures consistent physics and particle behavior regardless of rendering performance.
**Hypothetical Impact**: Significant improvement in simulation stability and consistency; no direct performance gain, but essential for a high-quality experience.

## Zero-Overhead Instrumentation
**Date**: 2024-05-22
**Technique**: RAII-based scope profiling gated by `PROFILING_ENABLED`.
**Reasoning**: To maintain a performance-first mindset, we need precise measurements of hot paths. These macros compile to nothing in release builds, ensuring no production overhead.
**Hypothetical Impact**: 0% runtime overhead in production; critical for identifying future bottlenecks.
