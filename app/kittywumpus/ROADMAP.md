# Kittywumpus Roadmap

This document outlines planned features and integration points for the Kittywumpus hybrid flight-sim/FPS game.

## Current Implementation (v0.1)

### Core Features
- [x] Flight mode with full paper plane controls
- [x] First-person mode with WASD movement and mouse look
- [x] Landing transition (CTRL at low altitude)
- [x] Takeoff transition (hold SPACE for 3 seconds)
- [x] Game state machine (Main Menu -> Flight -> FPS -> Flight -> Game Over)
- [x] HUD mode switching (flight gauges vs crosshair)
- [x] Enemy spawning system (aerial enemies in flight mode)

### Known Limitations
- FPS mode has no weapons yet
- No ground-based enemies
- Landing zones are anywhere below 2m altitude (no designated areas)
- No save/load system
- No settings menu
- No gamepad support

---

## Planned Features

### Phase 1: FPS Combat System

#### FPS Weapons
**Integration Points:**
- `FirstPersonController.h`: Add weapon handling methods
- `KittywumpusInputController.h`: Add `fire_fps`, `reload`, `weapon_switch_fps` fields
- `KittywumpusHandler.h`: Add FPS projectile spawning

**Planned Weapons:**
- Pistol (default, unlimited ammo, slow fire rate)
- SMG (pickup, high fire rate, limited ammo)
- Shotgun (pickup, spread pattern, limited ammo)
- Grenade launcher (pickup, arc trajectory)

**Tasks:**
- [ ] Create `FPSWeapon` base class
- [ ] Implement weapon pickup system
- [ ] Add ammo tracking to player state
- [ ] Create FPS-specific projectile types
- [ ] Add weapon model swapping for FPSRig

---

### Phase 2: Ground Enemies

**Integration Points:**
- `KittywumpusHandler::PreTimestep()`: Add ground enemy spawning when `is_flying_ == false`
- `GameState.cpp`: Add ground enemy awareness logic

**Planned Enemy Types:**
- Turret (stationary, fires at player)
- Patrol (walks a path, attacks when player spotted)
- Charger (runs at player when in range)
- Sniper (long-range, hides behind cover)

**Tasks:**
- [ ] Create `GroundEnemy` base class
- [ ] Implement line-of-sight detection
- [ ] Add cover system for enemies
- [ ] Create spawn point markers in terrain
- [ ] Balance damage/health for ground combat

---

### Phase 3: Mission System

**Integration Points:**
- `KittywumpusHandler.h`: Add `MissionManager` integration
- `GameState.h`: Add `MISSION_BRIEFING` and `MISSION_COMPLETE` states
- `KittywumpusPlane.h`: Add `SetObjective()` method

**Planned Mission Types:**
- Destroy all targets
- Escort/protect objective
- Reach destination
- Survive for X time
- Combined (multi-stage missions)

**Tasks:**
- [ ] Create `Mission` and `Objective` classes
- [ ] Implement mission UI (briefing, progress, completion)
- [ ] Add mission rewards (score multipliers, unlocks)
- [ ] Create mission sequence/campaign mode

---

### Phase 4: Designated Landing Zones

**Integration Points:**
- `KittywumpusPlane::CanLand()`: Check for landing zone proximity
- `KittywumpusHandler`: Spawn landing zone markers

**Features:**
- Visual landing zone markers (helipads, clearings)
- Landing zone discovery system
- Safe zones (no enemies spawn nearby)
- Repair/resupply at landing zones

**Tasks:**
- [ ] Create `LandingZone` entity type
- [ ] Add landing zone placement to terrain generation
- [ ] Implement "landing zone ahead" HUD indicator
- [ ] Add repair/resupply functionality

---

### Phase 5: Save/Load System

**Integration Points:**
- New `SaveManager` class
- `GameState.h`: Add save/load state handling
- `KittywumpusHandler`: Serialize entity positions

**Save Data:**
- Player position, health, score
- Current mission progress
- Unlocked weapons/upgrades
- Settings

**Tasks:**
- [ ] Design save file format (JSON or binary)
- [ ] Implement `SaveManager` class
- [ ] Add autosave at landing zones
- [ ] Create save slot UI

---

### Phase 6: Settings & Polish

**Settings Menu:**
- Graphics quality presets
- Audio volume sliders
- Control sensitivity
- Keybinding customization
- Gamepad configuration

**Polish:**
- [ ] Add control hints HUD
- [ ] Improve camera transitions
- [ ] Add particle effects for mode switches
- [ ] Create tutorial/training mission
- [ ] Gamepad support with vibration feedback

---

## Code Integration Points Reference

### Adding New Game States
1. Add enum value to `GameState.h`
2. Handle in `GameStateManager::Update()`
3. Add transition logic in `TransitionTo()`
4. Update HUD in `main.cpp` state change handler

### Adding New Enemy Types
1. Create header/cpp in `include/` and `src/`
2. Add spawning logic in `KittywumpusHandler::PreTimestep()`
3. Consider flight vs ground mode in spawn conditions

### Adding New Weapons
1. Create weapon class extending appropriate base
2. Add input binding in `KittywumpusInputController`
3. Add firing logic in plane or FPS controller
4. Update weapon selector HUD

### Adding New HUD Elements
1. Create element in `main.cpp`
2. Add show/hide logic for mode transitions
3. Update in appropriate input callback section

---

## Technical Notes

### Thread Safety
- `target_mutex_` protects target tracking
- `effect_mutex_` protects fire effects
- Use `EnqueueVisualizerAction()` for visualizer operations from entity threads

### Performance Considerations
- Enemy spawning uses occlusion culling
- Launcher placement uses chunk-based cooldowns
- Consider spatial partitioning for ground enemy collision

### Asset Requirements
- FPS weapon models needed
- Ground enemy models needed
- Landing zone marker assets
- UI icons for new features
