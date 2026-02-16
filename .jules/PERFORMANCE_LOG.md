# Performance Optimization Log

## 1. GPU-Accelerated Frustum Culling
-   **Technique**: Compute Shader + Indirect Rendering
-   **Impact**: Critical. Offloads per-instance visibility checks from CPU to GPU.
-   **Before**: CPU iterated over all instances, performed sphere-frustum tests, and emitted vertex-shader-culled degenerate triangles.
-   **After**: Compute shader (`shaders/cull.comp`) performs culling in parallel and populates a visible index buffer and indirect draw command. Vertex shader only runs for visible instances.
-   **Reasoning**: Replaces O(N) CPU work with O(1) CPU dispatch and O(N) highly parallel GPU work.

## 2. AZDO: Persistent Mapped Buffers
-   **Technique**: `glBufferStorage` + `glMapBufferRange` (Persistent/Coherent)
-   **Impact**: High. Eliminates `glBufferSubData` overhead and driver-side synchronization.
-   **Implementation**: Applied to `TrailRenderManager` (vertex data) and `InstanceManager` (matrix and color data).
-   **Before**: Frequent `glBufferSubData` calls caused driver overhead and potential CPU stalls.
-   **After**: CPU writes directly to GPU-visible memory.
-   **Reasoning**: Approaching Zero Driver Overhead (AZDO) by removing redundant data copying and synchronization.

## 3. Optimized Screen-Space Blur
-   **Technique**: Kawase Blur
-   **Impact**: Medium. Reduces GPU load for reflection rendering.
-   **Before**: 10-pass (20 draw calls) Gaussian blur.
-   **After**: 5-pass (5 draw calls) Kawase blur (`shaders/kawase_blur.frag`).
-   **Reasoning**: Kawase blur achieves similar or better visual quality with significantly fewer texture samples and draw calls.

## 4. Fixed Timestep Simulation
-   **Technique**: Deterministic Accumulator
-   **Impact**: High (Stability/Consistency).
-   **Implementation**: 1/60s fixed step for lights, fire, explosions, sound, and shockwaves.
-   **Reasoning**: Ensures simulation consistency across varying framerates and prevents "spiral of death" via accumulator capping.

## Instrumentation
-   **Macros**: `PROJECT_PROFILE_SCOPE`, `PROJECT_MARKER`
-   **Overhead**: Zero in production builds (macro-gated).
-   **Hot Paths Instrumented**: `Visualizer::Render`, `Visualizer::Update`, `InstanceManager::RenderModelGroup`, `InstanceManager::RenderDotGroup`.
