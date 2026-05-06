# GPU Resource Migration Plan

This document outlines the plan for migrating legacy OpenGL resources (SSBOs, UBOs, and Textures) to a modern, managed standard using Approach Zero Driver Overhead (AZDO) principles.

## 1. Modern Resource Standard

All GPU resources should meet the following criteria:

*   **Persistent**: Managed using `PersistentBuffer` or `PersistentTexture` wrappers.
*   **Coherent**: Utilizing `GL_MAP_COHERENT_BIT` for buffers to ensure CPU writes are immediately visible to the GPU without manual flushing.
*   **Read-Only (Optimized)**: Most resources should be treated as read-only from the shader perspective where possible, using triple-buffering to avoid stalls during updates.
*   **Centralized**: Registered with the `GpuResourceRegistry` (Resource Locator) and bound via `BindingSet`.

## 2. Resource Categories & Migration Strategies

### A. Uniform Buffers (UBOs)
**Strategy**: Replace `glGenBuffers` + `glBufferData` with `std::unique_ptr<PersistentBuffer<T>>`.
*   Use `GL_UNIFORM_BUFFER` target.
*   Implement triple-buffering (default in `PersistentBuffer`).
*   Update via `GetFrameDataPtr()` and advance every frame.
*   Bind using `BindingSet::UboRange`.

### B. Shader Storage Buffers (SSBOs)
**Strategy**: Use `PersistentBuffer<T>` for all CPU-to-GPU data paths.
*   Use `GL_SHADER_STORAGE_BUFFER` target.
*   For GPU-only buffers (e.g., visibility masks), still use `PersistentBuffer` but without persistent mapping if CPU access isn't required, or keep the mapping for easier debugging/readback.

### C. Static & Asset Textures
**Strategy**: Migrate `AssetManager` to use `PersistentTexture`.
*   Use `glTexStorage2D` (immutable storage) via the `PersistentTexture` constructor.
*   Register all loaded textures with `GpuResourceRegistry` using unique paths/IDs.

### D. Render Targets & LUTs
**Strategy**: Wrap FBO attachments and Look-Up Tables in `PersistentTexture`.
*   Ensures consistent binding and lifecycle management.
*   Publish to `GpuResourceRegistry` so post-processing effects can locate them without direct manager dependencies.

## 3. Detailed Inventory of Resources to Migrate

### Core Managers
| Manager | Resource Name | Type | Standard |
| :--- | :--- | :--- | :--- |
| `AtmosphereManager` | `_transmittanceLUT`, `_multiScatteringLUT`, etc. | Texture | `PersistentTexture` |
| `AtmosphereManager` | `_shCoeffsBuffer` | SSBO | `PersistentBuffer` |
| `TerrainRenderManager` | `biome_ubo_`, `terrain_data_ubo_` | UBO | `PersistentBuffer` |
| `TerrainRenderManager` | `chunk_grid_texture_`, `heightmap_texture_`, etc. | Texture | `PersistentTexture` |
| `TerrainRenderManager` | `probe_ssbo_`, `bake_ssbo_`, `instance_vbo_` | SSBO/VBO | `PersistentBuffer` |
| `WeatherManager` | `wind_data_ubo_` | UBO | `PersistentBuffer` |
| `WeatherManager` | `wind_texture_`, `lbm_wind_texture_` | Texture | `PersistentTexture` |
| `ShadowManager` | `shadow_map_array_` | Texture | `PersistentTexture` |
| `ShadowManager` | `shadow_ubo_` | UBO | `PersistentBuffer` |
| `NoiseManager` | `noise_texture_`, `curl_noise_texture_`, etc. | Texture | `PersistentTexture` |
| `HiZManager` | `hiz_texture_` | Texture | `PersistentTexture` |

### Effect Managers
| Manager | Resource Name | Type | Standard |
| :--- | :--- | :--- | :--- |
| `FireEffectManager` | `particle_buffer_`, `emitter_buffer_`, etc. | SSBO | `PersistentBuffer` |
| `DecorManager` | `block_validity_ssbo_`, `chunk_params_ssbo_` | SSBO | `PersistentBuffer` |
| `DecorManager` | `placement_globals_ubo_`, `decor_props_ubo_` | UBO | `PersistentBuffer` |
| `DecorManager` | `type.ssbo`, `type.visible_ssbo`, etc. | SSBO | `PersistentBuffer` |
| `GrassManager` | `grass_props_ubo_` | UBO | `PersistentBuffer` |
| `GrassManager` | `grass_instances_ssbo_`, `grass_indirect_buffer_` | SSBO | `PersistentBuffer` |
| `TrailRenderManager` | `points_ssbo_`, `instances_ssbo_`, `spine_ssbo_` | SSBO | `PersistentBuffer` |
| `MeshExplosionManager` | `ssbo_` | SSBO | `PersistentBuffer` |
| `SdfVolumeManager` | `ssbo_` | SSBO | `PersistentBuffer` |
| `ShockwaveManager` | `ubo_` | UBO | `PersistentBuffer` |

### Post-Processing & Compositing
| Manager | Resource Name | Type | Standard |
| :--- | :--- | :--- | :--- |
| `PostProcessingManager` | `pingpong_texture_` | Texture | `PersistentTexture` |
| `TemporalAccumulator` | `_historyTextures` | Texture | `PersistentTexture` |
| `AtmosphereEffect` | `low_res_texture_`, `temporal_textures_` | Texture | `PersistentTexture` |
| `BloomEffect` | `_bloomTexture` | Texture | `PersistentTexture` |
| `BloomEffect` | `_exposureSsbo` | SSBO | `PersistentBuffer` |
| `SceneCompositor` | `color_tex_`, `depth_tex_`, `normal_tex_`, etc. | Texture | `PersistentTexture` |

## 4. Integration Steps

1.  **Publishing**: Every manager must call `GpuResourceRegistry::Publish[Texture|Ubo|Ssbo]` immediately after resource creation.
2.  **Locating**: Systems requiring external resources must use `GpuResourceRegistry::Instance().Get[Texture|Ubo|Ssbo](slot)` instead of storing raw GL handles.
3.  **Binding**: Replace manual `glActiveTexture` + `glBindTexture` chains with `BindingSet`.
    *   Example:
        ```cpp
        BindingSet()
            .Texture(Constants::TextureUnit::ShadowMaps(), shadow_mgr.GetShadowMapArray())
            .Ubo(Constants::UboBinding::Lighting(), lighting_pb)
            .Apply();
        ```

## 5. Roadmap

1.  **Phase 1: Core Infrastructure** (Complete)
    *   `PersistentBuffer` and `PersistentTexture` classes implemented.
    *   `GpuResourceRegistry` and `BindingSet` implemented.
2.  **Phase 2: Annotation** (Complete)
    *   All legacy resource locations identified and marked with instructions.
3.  **Phase 3: Core Migration**
    *   Migrate `TerrainRenderManager`, `ShadowManager`, and `AtmosphereManager`.
    *   Integrate with `Visualizer` frame loop.
4.  **Phase 4: Effects & Post-Processing Migration**
    *   Migrate all particle systems and screen-space effects.
5.  **Phase 5: Cleanup**
    *   Remove legacy `glGen*` and `glBind*` calls from the hot path.
    *   Remove `TypedBuffer` and other non-persistent utility classes.
