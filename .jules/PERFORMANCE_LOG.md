# Performance Log

## Optimizations Implemented

### 1. Zero-Overhead Instrumentation
- **Technique**: Macro-gated RAII scope profiling and OpenGL debug markers.
- **Rationale**: Provides precise visibility into GPU workload partitioning (Shadow Pass, Reflection Pass, Main Pass) without any impact on release builds.
- **Impact**: Zero runtime cost when `PROFILING_ENABLED` is off. Enables optimization via empirical evidence.

### 2. AZDO: Persistent Mapped Buffers
- **Technique**: `GL_ARB_buffer_storage` with `GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT`.
- **Rationale**: Replaces frequent `glBufferSubData` calls for UBOs (Lighting, Temporal Data, Visual Effects) and Instance VBOs. Eliminates CPU-GPU synchronization stalls and driver-side memory management overhead.
- **Estimated Impact**: 5-15% reduction in CPU frame time, especially in scenes with many light updates or moving instances.

### 3. Bindless Textures
- **Technique**: `GL_ARB_bindless_texture` using `glGetTextureHandleARB`.
- **Rationale**: Bypasses the limited and slow "Texture Unit" binding system. Allows models with many meshes and unique textures to be drawn without state changes.
- **Estimated Impact**: Significant reduction in driver overhead for complex models. Enables the use of MDI across diverse texture sets.

### 4. Multi-Draw Indirect (MDI) & Unified Buffers
- **Technique**: `glMultiDrawElementsIndirect` with unified vertex/index buffers per model.
- **Rationale**: Reduces hundreds of `glDrawElements` calls per model to a single draw command. Combines with bindless textures to handle multi-material models in one shot.
- **Estimated Impact**: Up to 40% reduction in driver-side CPU time for models with many meshes (e.g., foliage, detailed vehicles).

### 5. GPU-Accelerated Frustum Culling
- **Technique**: Compute shader for per-instance sphere-frustum tests and stream compaction.
- **Rationale**: Offloads frustum culling from the CPU to the GPU. Eliminates the need to send hidden instance data to the vertex shader.
- **Estimated Impact**: Massive performance gain in scenes with high instance counts (e.g., thousands of boids) where only a fraction is visible.

## Verification
- `make check`: PASSED (C++ integrity)
- `make glcheck`: PASSED (Shader validation)
- Verified with `model_demo` and instrumentation infrastructure.
