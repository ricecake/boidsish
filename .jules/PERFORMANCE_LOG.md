# Boidsish Performance Log

## [Uncommitted] GPU-Driven Optimizations & Stability

### 1. Fixed Timestep Simulation
- **Technique**: Implement an accumulator-based fixed timestep loop (1/60s) in the main update cycle.
- **Why**: Ensures simulation-heavy systems (physics, weather, particles) behave deterministically and remain stable even if the rendering framerate fluctuates.
- **Impact**:
  - *Before*: Simulation speed tied directly to framerate; potential instability at very high/low FPS.
  - *After*: Guaranteed 60Hz update rate for logic; smooth interpolation possibility.

### 2. GPU-Based Trail Generation
- **Technique**: Replaced CPU-side `std::deque` point tracking with a GPU compute shader (`trail_update.comp`) that manages the ring buffer directly in an SSBO.
- **Why**: Reduces CPU-GPU bus traffic. Previously, the entire trail history for every object was uploaded every frame. Now, only the new position is sent.
- **Impact**:
  - *Before*: O(N*M) data upload per frame (N=trails, M=length).
  - *After*: O(N) data upload per frame; significantly reduced driver overhead and PCIe bandwidth usage.

### 3. Unified GPU Culling
- **Technique**: Enhanced `occlusion_cull.comp` to perform view frustum culling (AABB-plane test) before the Hi-Z occlusion test.
- **Why**: Offloads thousands of AABB tests from the CPU to the highly parallel GPU. Consolidates two culling passes into one compute dispatch.
- **Impact**:
  - *Before*: CPU-side frustum culling.
  - *After*: Zero CPU cost for frustum culling; improved dispatch efficiency by early-rejecting frustum-culled objects before expensive Hi-Z samples.

### 4. Zero-Overhead Profiling
- **Technique**: Added `FixedUpdate` and `OcclusionCullDispatch` scopes using `PROJECT_PROFILE_SCOPE`.
- **Why**: Maintains observability of high-frequency systems without impacting production performance.
- **Impact**: Granular timing data available in debug builds; completely stripped in release.
