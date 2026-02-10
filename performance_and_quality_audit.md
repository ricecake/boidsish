# Performance and Quality Audit

This document outlines key areas for performance enhancement and code quality improvements within the application. Each section details a specific issue, its impact, a proposed solution, and the relevant code location for future refactoring.

## 1. GPU-Accelerated Frustum Culling

-   **Issue**: View frustum culling is currently performed on the CPU in `src/graphics.cpp`. As the number of objects in the scene increases, this calculation can become a bottleneck, consuming valuable CPU cycles that could be used for simulation or other tasks.
-   **Proposed Solution**: Offload frustum culling to a compute shader. This would involve passing the view-projection matrix and a list of object bounding volumes (e.g., spheres or AABBs) to a compute shader, which would then determine visibility and output a compact list of visible objects. This approach leverages the massive parallelism of the GPU to perform the culling calculations much faster than the CPU.
-   **Code Location**: `src/graphics.cpp`, `VisualizerImpl::CalculateFrustum`

## 2. Optimized Screen-Space Reflections and Blur [COMPLETED]

-   **Issue**: The previous planar reflection effect used a multi-pass Gaussian blur (`RenderBlur`) that required 10 passes. This was computationally expensive, consuming significant GPU time and memory bandwidth due to multiple render target swaps and texture reads.
-   **Solution Implemented**: Replaced the dual-pass planar reflection system with a **Hi-Z Screen-Space Stochastic Reflection (SSSR)** system.
    -   Implemented a Hierarchical-Z (Hi-Z) mipmap generator (`hiz_depth.comp`) to accelerate ray-marching.
    -   Developed a reusable **Temporal Reprojection** system (`temporal_reprojection.frag`) with variance clipping to stabilize stochastic noise.
    -   Unified the PBR pipeline by outputting roughness/metallic data to the G-Buffer from `plane.frag` and `terrain.frag`.
    -   Purged the legacy planar reflection rendering and multi-pass blur code from `src/graphics.cpp`.
-   **Status**: Completed. SSSR is now integrated into the main post-processing pipeline.

## 3. GPU-Based Trail Generation

-   **Issue**: Object trails are currently generated on the CPU by storing a history of points and constructing a mesh. This approach is not scalable and becomes a performance bottleneck as the number of objects with trails increases. The constant creation and updating of vertex buffers on the CPU can lead to significant overhead and bus traffic between the CPU and GPU.
-   **Proposed Solution**: Move the trail generation logic to the GPU. This can be achieved using a transform feedback buffer or a compute shader. The simulation would write the current position of each object to a buffer, and a shader would be responsible for generating the trail geometry (e.g., a triangle strip) based on the history of positions. This would dramatically reduce the amount of data that needs to be transferred from the CPU to the GPU each frame.
-   **Code Location**: `src/graphics.cpp`, `VisualizerImpl::RenderSceneObjects` (trail creation logic)

## 4. Fixed Timestep for Simulation Stability

-   **Issue**: The simulation currently runs on a variable timestep based on the frame rate (`delta_time`). This can lead to instability and non-deterministic behavior, especially at low or fluctuating frame rates. The physics and behavior of the simulation can change depending on the performance of the rendering, which is undesirable.
-   **Proposed Solution**: Implement a fixed timestep for the simulation logic. This involves running the simulation in discrete, fixed-size time steps (e.g., 1/60th of a second) independent of the frame rate. An accumulator would be used to keep track of the elapsed real time, and the simulation would be updated as many times as needed to catch up. This change would improve the stability and reproducibility of the simulation, ensuring that it behaves consistently across different hardware and frame rates.
-   **Code Location**: `src/graphics.cpp`, `Visualizer::Update` (where `simulation_time` is advanced).
