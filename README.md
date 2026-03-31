# Boidsish

A 3D visualization and simulation framework in C++ that started as a boids flocking demo and kept growing legs. Now features GPU-driven rendering, procedural terrain with tessellation, atmospheric scattering, compute-shader particle effects, and enough post-processing to make a film student nervous.

Still has boids, though.

---

## What It Does

Boidsish provides a programmable 3D scene where you define what exists and how it behaves, and the framework handles rendering, terrain, lighting, shadows, trails, audio, and camera. You write functions that produce shapes and entities; it draws them at 60+ fps with PBR materials, cascaded shadow maps, and hierarchical occlusion culling.

It's built for experimentation — flocking simulations, procedural creatures, particle art, terrain exploration, physics demos, or whatever you want to throw into a 3D space and watch happen.

## Features

**Rendering**
- GPU-driven pipeline: Multi-Draw Indirect with megabuffer, per-draw visibility via compute shader, Hi-Z occlusion culling
- PBR materials with roughness, metallic, AO, and emissive channels
- Cascaded shadow mapping with debt-based update scheduling
- Post-processing chain: GTAO, bloom, auto-exposure, film grain, temporal accumulation
- Decor system: GPU-placed vegetation with per-instance wind, terrain-aware occlusion, and compute-shader placement

**Terrain**
- Procedural generation with biome blending and noise-based features
- GPU tessellation with adaptive screen-space LOD, silhouette boost, and curvature boost
- Patch-based compute-shader culling (8×8 patches per chunk, frustum + terrain-raycast occlusion)
- Runtime deformation (craters, flattening) with async regeneration
- Heightmap texture array streaming with non-destructive growth

**Atmosphere & Environment**
- Physically-based sky with Rayleigh + Mie scattering via precomputed LUTs
- Procedural volumetric clouds
- Dynamic weather system
- Moon and celestial rendering

**Effects**
- GPU compute fire/explosion particles with multiple styles and curl noise turbulence
- Shockwave ring distortion (terrain + object displacement)
- Mesh explosion debris with per-fragment physics
- SDF volume effects
- GPU-tessellated trails with iridescence, rocket mode, and PBR materials

**Entities & Simulation**
- Template-based entity system with shape generation callbacks
- Procedural walking creatures with inverse kinematics
- Flocking behaviors, steering probes, spatial octree queries
- Spline-based paths with multiple camera modes (free, tracking, chase, path-follow, ambient cinematic)

**Audio**
- OpenAL-based 3D positional audio
- Sound effect management with listener tracking

**Tools**
- ImGui widget system (profiler, environment controls, effect tweaking)
- 20 test suites covering core systems
- Built-in frame profiler with per-section GPU timing
- 67 example programs

## Building

**Requirements:** CMake 3.16+, C++23 compiler, OpenGL 4.3+

**macOS:**
```bash
brew install glfw glew glm assimp
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install libglfw3-dev libglew-dev libglm-dev libassimp-dev libopenal-dev
```

**Build:**
```bash
mkdir build && cd build
cmake ..
cmake --build . --target boidsish
```

**Build an example:**
```bash
cmake --build . --target fire_demo
./examples/fire_demo/fire_demo
```

**Run tests:**
```bash
cmake --build . --target test_terrain
./tests/test_terrain
```

## Quick Start

```cpp
#include "graphics.h"
using namespace Boidsish;

int main() {
    Visualizer viz(1280, 720, "Hello Boidsish");

    // Add some shapes each frame
    viz.SetShapeFunction([](float time) {
        std::vector<std::shared_ptr<Shape>> shapes;

        auto dot = std::make_shared<Dot>();
        dot->SetPosition(cos(time) * 5.0f, sin(time * 0.7f) * 2.0f, sin(time) * 5.0f);
        dot->SetColor(1.0f, 0.5f, 0.2f);
        dot->SetTrailLength(40);
        shapes.push_back(dot);

        return shapes;
    });

    viz.Run();
    return 0;
}
```

For terrain, decor, fire, audio, entities, and more — see the `examples/` directory. There are 67 of them.

## Camera Controls

| Key | Action |
|-----|--------|
| WASD | Move horizontally |
| Space / Shift | Move up / down |
| Mouse | Look around |
| Scroll | Adjust speed |
| 1-5 | Camera modes (free, auto, tracking, chase, path) |
| ESC | Exit |

## Architecture Overview

```
Visualizer
├── RenderQueue (parallel packet generation, MDI batching)
├── ShadowManager (4-cascade CSM, two-phase render)
├── TerrainGenerator → TerrainRenderManager (GPU tessellation, patch culling)
├── DecorManager (GPU placement, block validity, terrain occlusion)
├── HiZManager (depth pyramid for occlusion culling)
├── AtmosphereManager (LUT-based scattering)
├── FireEffectManager (GPU compute particles)
├── TrailRenderManager (GPU tessellation)
├── MeshExplosionManager (fragment physics)
├── ShockwaveManager (ring distortion)
├── PostProcessingManager (GTAO, bloom, exposure, grain)
├── AudioManager (OpenAL 3D audio)
├── LightManager (10 lights, PBR evaluation)
└── EntityHandler (shape generation, entity lifecycle)
```

Shaders live in `shaders/` and use `#include` for shared code (`helpers/lighting.glsl`, `helpers/terrain_shadows.glsl`, `helpers/noise.glsl`, etc.). Compute shaders handle culling, placement, particle simulation, trail generation, Hi-Z pyramid building, and atmosphere LUT precomputation.

## Project Structure

```
include/           Headers for all systems
src/               Implementation files
shaders/           GLSL shaders (vert, frag, comp, tcs, tes, glsl helpers)
external/          Third-party libraries (ImGui, stb, FastNoise2, etc.)
examples/          67 example programs
tests/             20 test suites
assets/            Models, textures
```

## Dependencies

Bundled in `external/`: ImGui, stb_image, FastNoise2, meshoptimizer, task-thread-pool, libmorton, Bonxai, lygia, QuickJS (optional).

System: OpenGL, GLFW, GLEW, GLM, ASSIMP, OpenAL-Soft.

## License

This project is provided for educational and development purposes.
