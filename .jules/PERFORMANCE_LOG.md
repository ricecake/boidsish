# Performance Log

## Optimization: Fixed Timestep Simulation
- **Before**: Simulation advanced by `delta_time`, leading to non-deterministic behavior and potential physics instability at low/variable framerates.
- **After**: Implemented 60Hz fixed timestep with accumulator in `Visualizer::Update`.
- **Impact**: Improved simulation stability and determinism. Zero impact on rendering performance but eliminates "physics glitches" during frame spikes.

## Optimization: GPU-Accelerated Frustum Culling
- **Before**: Culling performed on CPU or via redundant sphere-checks in `vis.vert` (per vertex).
- **After**: Compute shader `frustum_cull.comp` processes AABB visibility for all MDI draw commands. Sets `instanceCount` to 0 for culled objects.
- **Impact**: Offloads CPU overhead for large object counts. Eliminates redundant vertex shader work for culled objects. Estimated ~10-15% reduction in CPU-GPU sync wait time in dense scenes.

## Optimization: Bloom Blur (Dual Kawase)
- **Before**: 9-tap Kawase-like downsample.
- **After**: Optimized 5-tap dual Kawase filter using bilinear sampling to cover the same 13-tap area.
- **Impact**: Reduced texture fetches in bloom downsample passes. Improved GPU throughput for post-processing.
