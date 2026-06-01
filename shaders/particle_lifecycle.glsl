#ifndef PARTICLE_LIFECYCLE_GLSL
#define PARTICLE_LIFECYCLE_GLSL

#include "helpers/constants.glsl"
#include "helpers/terrain_common.glsl"
#include "frustum.glsl"

#define worldScale u_terrainParams.y

void spawnDustParticle(
	inout Particle p,
	uint           gid,
	float          time,
	vec3           viewPos,
	vec3           viewDir,
	sampler3D      curlTexture,
	int            num_chunks,
	sampler2DArray heightmapArray
);
#include "particle_behavior.glsl"
#include "particle_helpers.glsl"
#include "particle_types.glsl"
#include "visual_effects.glsl"

bool updateLifetime(
	inout Particle p,
	uint           gid,
	int            emitter_index,
	int            num_emitters,
	float          dt,
	vec3           viewPos,
	vec3           viewDir
) {
	// Hijack/Clear/Rebalance Logic
	// If the emitter_index for this slot has changed, kill the particle immediately to allow re-balancing.
	if (emitter_index != -1 && num_emitters > 0 && emitter_index < num_emitters) {
		Emitter emitter = emitters[emitter_index];
		if (emitter.request_clear != 0 || p.emitter_id != emitter.id) {
			p.pos.w = 0.0;
		}
	} else if (emitter_index == -1 && p.emitter_id >= 0) {
		// Particle was an emitter particle, but now its slot is ambient.
		p.pos.w = 0.0;
	}

	// Aging
	p.pos.w -= dt;

	// View-distance/direction culling
	float cullDist = (p.style == STYLE_RAIN || p.style == STYLE_SNOW) ? 150.0 : 15.0;
	if (distance(p.pos.xz, viewPos.xz) > cullDist && dot(p.pos.xz - viewPos.xz, viewDir.xz) < -0.5) {
		p.pos.w = 0;
	}

	return p.pos.w <= 0.0;
}

void spawnRocketTrail(inout Particle p, vec3 spawn_pos, float lifeTimeModifier, Emitter emitter, vec3 random_vec, vec2 seed) {
	p.pos = vec4(spawn_pos, kExhaustLifetime * lifeTimeModifier);
	vec3 perpendicular = normalize(cross(emitter.direction, random_vec));
	vec3 spread_dir = mix(emitter.direction, perpendicular, kExhaustSpread * rand(seed.yx));
	p.vel = vec4(emitter.velocity + normalize(spread_dir) * kExhaustSpeed * rand(seed), 15.0);
}

void spawnExplosion(inout Particle p, vec3 spawn_pos, float lifeTimeModifier, float dt, sampler3D curlTexture, vec3 random_vec) {
	p.pos = vec4(spawn_pos, kExplosionLifetime * lifeTimeModifier);
	p.vel = vec4(
		normalize(random_vec) * kExplosionSpeed * 0.5 +
			abs(fbmCurlMagnitude(p.pos.xyz, dt, curlTexture)),
		60.0
	);
}

void spawnFire(inout Particle p, vec3 spawn_pos, float lifeTimeModifier, vec3 random_vec) {
	p.pos = vec4(spawn_pos + random_vec * 0.1, kFireLifetime * lifeTimeModifier);
	p.vel = vec4(0, kFireSpeed, 0, 25.0);
}

void spawnSparks(inout Particle p, vec3 spawn_pos, Emitter emitter, vec3 random_vec, vec2 seed) {
	p.pos = vec4(spawn_pos, kSparksLifetime * (rand(seed) * 0.5 + 0.5));
	p.vel = vec4(emitter.velocity + normalize(random_vec) * kSparksSpeed * rand(seed.yx), 4.0);
}

void spawnGlitter(inout Particle p, vec3 spawn_pos, float lifeTimeModifier, Emitter emitter, vec3 random_vec, vec2 seed) {
	p.pos = vec4(spawn_pos, kGlitterLifetime * lifeTimeModifier);
	vec3 dir = length(emitter.direction) > 0.001 ? normalize(emitter.direction) : vec3(0, 1, 0);
	vec3 spread_dir = normalize(dir + (random_vec * kGlitterSpread));
	p.vel = vec4(emitter.velocity + spread_dir * kGlitterSpeed * (0.5 + 0.5 * rand(seed)), 6.0);
}

void spawnEmitterBubbles(inout Particle p, vec3 spawn_pos, vec2 seed) {
	p.pos = vec4(spawn_pos, 3.0 + rand(seed) * 2.0);
	p.vel = vec4(rand3(seed) * 0.2, 0.0);
	p.vel.y += 0.2;
	p.vel.w = 15.0;
}

void spawnEmitterFireflies(inout Particle p, vec3 spawn_pos, vec2 seed) {
	p.pos = vec4(spawn_pos, 5.0 + rand(seed) * 5.0);
	p.vel = vec4(rand3(seed) * 0.1, 0.0);
	p.vel.w = 15.0;
}

void spawnDebug(inout Particle p, vec3 spawn_pos) {
	p.pos = vec4(spawn_pos, 10.0);
	p.vel = vec4(0.0, 0.0, 0.0, 8.0);
}

void spawnCinder(inout Particle p, vec3 spawn_pos, vec3 random_vec, vec2 seed) {
	p.pos = vec4(spawn_pos, kCinderLifetime * (0.8 + 0.4 * rand(seed)));
	p.vel = vec4(normalize(random_vec) * kCinderSpeed * rand(seed.yx), 12.0);
}

void spawnEmitterParticle(
	inout Particle p,
	uint           gid,
	int            emitter_index,
	float          time,
	float          dt,
	sampler3D      curlTexture
) {
	Emitter emitter = emitters[emitter_index];
	if (emitter.is_active == 0) return;

	p.emitter_index = emitter_index;
	p.emitter_id = emitter.id;
	p.style = emitter.style;
	p.origin.xyz = emitter.position;
	p.origin.w = 1.0; // Intensity

	vec2  seed = vec2(float(gid), time);
	vec3  random_vec = rand3(seed) * 2.0 - 1.0;
	float lifeTimeModifier = rand(seed);

	vec3 spawn_pos = emitter.position;
	bool skip_spawn = false;

	if (emitter.use_slice_data != 0) {
		float window = clamp(emitter.slice_area * 0.0001, 0.0005, 0.05);
		if (abs(lifeTimeModifier - emitter.sweep) > window) {
			skip_spawn = true;
		} else {
			spawn_pos = slice_data[emitter.slice_data_offset + (gid % emitter.slice_data_count)].xyz;
		}
	} else if (emitter.type == 4) { // Model but no slice data
		skip_spawn = true;
	} else if (emitter.type == 1) { // Box
		vec3 r = rand3(seed) - 0.5;
		vec3 rel_p = r * emitter.dimensions;
		if (length(emitter.direction) > 0.001) {
			vec3  n = normalize(emitter.direction);
			float h = dot(rel_p, n);
			float extent = dot(abs(n), emitter.dimensions * 0.5);
			float normalized_h = (h / (extent * 2.0)) + 0.5;
			if (normalized_h > emitter.sweep) {
				skip_spawn = true;
			}
		} else if (r.y + 0.5 > emitter.sweep) {
			skip_spawn = true;
		}
		if (!skip_spawn)
			spawn_pos += rel_p;
	} else if (emitter.type == 2) { // Sphere
		vec3 r = normalize(random_vec) * rand(seed.yx);
		if (r.y > (emitter.sweep * 2.0 - 1.0)) {
			skip_spawn = true;
		}
		if (!skip_spawn)
			spawn_pos += r * emitter.dimensions.x;
	} else if (emitter.type == 3) { // Beam
		float r = rand(seed);
		if (r > emitter.sweep) {
			skip_spawn = true;
		}
		if (!skip_spawn)
			spawn_pos += normalize(emitter.direction) * r * emitter.dimensions.x;
	}

	if (!skip_spawn) {
		if (p.style == STYLE_ROCKET_TRAIL) {
			spawnRocketTrail(p, spawn_pos, lifeTimeModifier, emitter, random_vec, seed);
		} else if (p.style == STYLE_EXPLOSION) {
			spawnExplosion(p, spawn_pos, lifeTimeModifier, dt, curlTexture, random_vec);
		} else if (p.style == STYLE_FIRE) {
			spawnFire(p, spawn_pos, lifeTimeModifier, random_vec);
		} else if (p.style == STYLE_SPARKS) {
			spawnSparks(p, spawn_pos, emitter, random_vec, seed);
		} else if (p.style == STYLE_GLITTER) {
			spawnGlitter(p, spawn_pos, lifeTimeModifier, emitter, random_vec, seed);
		} else if (p.style == STYLE_BUBBLES) {
			spawnEmitterBubbles(p, spawn_pos, seed);
		} else if (p.style == STYLE_FIREFLIES) {
			spawnEmitterFireflies(p, spawn_pos, seed);
		} else if (p.style == STYLE_DEBUG) {
			spawnDebug(p, spawn_pos);
		} else if (p.style == STYLE_CINDER) {
			spawnCinder(p, spawn_pos, random_vec, seed);
		}
	}
}

bool spawnAmbientParticle(
	inout Particle p,
	uint           gid,
	float          time,
	float          ambient_density,
	uint           ambient_particle_scale,
	int            num_chunks,
	vec3           viewPos,
	vec3           viewDir,
	float          nightFactor,
	sampler3D      curlTexture,
	sampler2DArray heightmapArray,
	sampler2DArray biomeMap,
	float          cellSize,
	uint           gridSize
) {
	uint ambient_pool_size = uint(ambient_density * float(ambient_particle_scale));
	vec2 spawnSeed = vec2(float(gid) * 0.123, time * 0.456);
	if (gid < (particles.length() - ambient_pool_size) || num_chunks <= 0) return false;

	uint  particleSeed = hash(gid ^ uint(time * 10.0));
	float r_dist = randomFloat(particleSeed);
	float dist = sqrt(100.0 + r_dist * 249900.0) * worldScale;

	float u_angle = randomFloat(hash(particleSeed)) * 2.0 - 1.0;
	float shaped_angle = mix(u_angle, u_angle * u_angle * u_angle, 0.6);
	float offsetAngle = shaped_angle * 0.78539816339;

	float c = cos(offsetAngle);
	float s = sin(offsetAngle);
	mat2  rotationMatrix = mat2(c, -s, s, c);
	vec2  baseDir = normalize(viewDir.xz);
	vec2  rotatedOffset = (rotationMatrix * baseDir) * dist;
	vec3  pos = vec3(viewPos.x + rotatedOffset.x, 0.0, viewPos.z + rotatedOffset.y);

	int       chunk_idx = -1;
	ChunkInfo chunk;
	for (int i = 0; i < num_chunks; i++) {
		if (pos.x >= chunks[i].worldOffset.x && pos.x < chunks[i].worldOffset.x + chunks[i].size &&
		    pos.z >= chunks[i].worldOffset.y && pos.z < chunks[i].worldOffset.y + chunks[i].size) {
			chunk = chunks[i];
			chunk_idx = i;
			break;
		}
	}

	if (chunk_idx != -1 && distance(pos.xz, viewPos.xz) <= 750.0 * worldScale) {
		vec2  uv = (pos.xz - chunk.worldOffset) / chunk.size;
		vec4  terrain = texture(heightmapArray, vec3(uv, chunk.slice));
		float height = terrain.r + 0.05;
		vec2  biome_data = texture(biomeMap, vec3(uv, chunk.slice)).rg;

		int biome_idx = int(biome_data.x * 255.0 + 0.5);
		if (rand(spawnSeed + 0.987) < biome_data.y) {
			biome_idx = min(biome_idx + 1, 7);
		}

		vec3 jitter = (rand3(spawnSeed + 0.1) - 0.5) * 4.0;
		pos.x += jitter.x;
		pos.z += jitter.z;
		pos += 5.0 * viewDir;

		bool valid_biome = (biome_idx >= 0 && biome_idx <= 4) || biome_idx == 7;
		if (valid_biome) {
			// Define weighted probabilities for inter-compatible particles
			// Birds(0), Leaves(1), Petals(2), Bubbles(3), Fireflies(4), Fairy(5)
			float weights[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

			if (biome_idx == 0) { // Ocean/Water (Bubbles)
				weights[3] = 1.0;
			} else if (biome_idx != 7) { // Land biomes (not mountains)
				if (nightFactor > 0.5) {
					weights[4] = 0.8; // Fireflies at night
					weights[5] = 0.2; // Fairies at night
				} else {
					weights[0] = 0.05; // Birds
					if (biome_idx == 4) { // Forest
						weights[1] = 0.4; // Leaves
						weights[2] = 0.4; // Petals
						weights[5] = 0.2; // Fairies in forest
					} else {
						weights[1] = 0.8; // Leaves
						weights[2] = 0.15; // Petals
						weights[5] = 0.05; // Rare fairies elsewhere
					}
				}
			}

			// Quota enforcement: if at or over limit, weight is zero
			if (stats.count_birds >= stats.limit_birds) weights[0] = 0.0;
			if (stats.count_leaves >= stats.limit_leaves) weights[1] = 0.0;
			if (stats.count_petals >= stats.limit_petals) weights[2] = 0.0;
			if (stats.count_bubbles >= stats.limit_bubbles) weights[3] = 0.0;
			if (stats.count_fireflies >= stats.limit_fireflies) weights[4] = 0.0;
			if (stats.count_fairies >= stats.limit_fairies) weights[5] = 0.0;

			float total_weight = 0.0;
			for (int i = 0; i < 6; i++) total_weight += weights[i];
			if (total_weight <= 0.0) return false;

			// Pick type based on weighted probability
			float r = rand(spawnSeed + 6.6) * total_weight;
			int   selected_style = -1;
			float cumulative = 0.0;
			for (int i = 0; i < 6; i++) {
				cumulative += weights[i];
				if (r <= cumulative) {
					if (i == 0) { if (atomicAdd(stats.count_birds, 1) < stats.limit_birds) selected_style = STYLE_BIRDS; else atomicAdd(stats.count_birds, 0xFFFFFFFFU); }
					else if (i == 1) { if (atomicAdd(stats.count_leaves, 1) < stats.limit_leaves) selected_style = STYLE_LEAF; else atomicAdd(stats.count_leaves, 0xFFFFFFFFU); }
					else if (i == 2) { if (atomicAdd(stats.count_petals, 1) < stats.limit_petals) selected_style = STYLE_PETAL; else atomicAdd(stats.count_petals, 0xFFFFFFFFU); }
					else if (i == 3) { if (atomicAdd(stats.count_bubbles, 1) < stats.limit_bubbles) selected_style = STYLE_BUBBLES; else atomicAdd(stats.count_bubbles, 0xFFFFFFFFU); }
					else if (i == 4) { if (atomicAdd(stats.count_fireflies, 1) < stats.limit_fireflies) selected_style = STYLE_FIREFLIES; else atomicAdd(stats.count_fireflies, 0xFFFFFFFFU); }
					else if (i == 5) { if (atomicAdd(stats.count_fairies, 1) < stats.limit_fairies) selected_style = STYLE_FAIRY; else atomicAdd(stats.count_fairies, 0xFFFFFFFFU); }
					break;
				}
			}

			if (selected_style == -1) return false;

			float total_lifetime = 10.0 + rand(spawnSeed + 4.4) * 5.0;
			float skipped_time = rand(spawnSeed + 7.7) * total_lifetime;

			p.emitter_id = -1;
			p.emitter_index = biome_idx;
			p.style = selected_style;

			p.pos = vec4(
				pos.x,
				height + 1.0 + rand(spawnSeed + 3.3) * 2.0,
				pos.z,
				total_lifetime - skipped_time
			);
			p.vel = vec4(rand3(spawnSeed + 5.5) * 0.5, 15.0);
			p.origin.xyz = p.pos.xyz;
			p.origin.w = 0.0; // Last twinkle time

			if (selected_style == STYLE_FIREFLIES || selected_style == STYLE_FAIRY) {
				p.phase = 3.0 + 2.0 * fract(randomFloat(hash(particleSeed)));
				p.counter = 0.0;
			} else if (selected_style == STYLE_BIRDS) {
				p.phase = rand(spawnSeed + 8.8) * 6.28;
				p.pos.y = height + 0.1;
				p.pos.w = 60;
				p.vel.w = 35.0; // Bird size
			}

			if (skipped_time > 0.001) {
				updateAmbientParticle(
					p,
					skipped_time,
					time - skipped_time,
					viewPos,
					viewDir,
					cellSize,
					gridSize,
					curlTexture,
					num_chunks,
					heightmapArray
				);
			}
			return true;
		}
	}
	return false;
}

void spawnDustParticle(
	inout Particle p,
	uint           gid,
	float          time,
	vec3           viewPos,
	vec3           viewDir,
	sampler3D      curlTexture,
	int            num_chunks,
	sampler2DArray heightmapArray
) {
	vec2 spawnSeed = vec2(float(gid) * 0.123, time * 0.456);
	uint particleSeed = hash(gid ^ uint(time * 10.0));

	// Spawn in a ring around the camera
	float r_dist = randomFloat(particleSeed);
	float dist = 2.0 + r_dist * 18.0;

	float u_angle = randomFloat(hash(particleSeed)) * 2.0 * PI;
	float v_angle = (randomFloat(hash(hash(particleSeed))) * 2.0 - 1.0) * 0.5; // Bias toward horizontal

	vec3 offset = vec3(cos(u_angle) * cos(v_angle), sin(v_angle), sin(u_angle) * cos(v_angle)) * dist;
	vec3 pos = viewPos + offset;

	p.emitter_id = -1;
	p.emitter_index = -1;
	p.style = STYLE_DUST;
	p.pos = vec4(pos, 5.0 + rand(spawnSeed));
	p.vel = vec4(rand3(spawnSeed + 0.22) * 0.1, 1.0); // Small size
	p.origin.xyz = p.pos.xyz;
	p.origin.w = 0.0;
}

bool spawnEnvironmentalQueue(inout Particle p, uint gid, float time, float ambient_density, uint ambient_particle_scale, vec3 viewPos) {
	uint ambient_pool_size = uint(ambient_density * float(ambient_particle_scale));
	if (gid < (particles.length() - ambient_pool_size)) return false;

	vec2 seed = vec2(float(gid) * 0.123, time * 0.456);

	// Random spawn chance to spread spawning over time
	if (rand(seed + 0.77) > 0.1) return false;

	// Determine which style to spawn based on weather and quotas
	int selected_style = -1;
	float dust_threshold = clamp(1.0 - wetness, 0.0, 1.0);

	if (rain_intensity > 0.01) {
		if (atomicAdd(stats.count_rain, 1) < stats.limit_rain) selected_style = STYLE_RAIN;
		else atomicAdd(stats.count_rain, 0xFFFFFFFFU);
	} else if (snow_intensity > 0.01) {
		if (atomicAdd(stats.count_snow, 1) < stats.limit_snow) selected_style = STYLE_SNOW;
		else atomicAdd(stats.count_snow, 0xFFFFFFFFU);
	} else if (dust_threshold > 0.01) {
		if (atomicAdd(stats.count_dust, 1) < stats.limit_dust) selected_style = STYLE_DUST;
		else atomicAdd(stats.count_dust, 0xFFFFFFFFU);
	}

	if (selected_style != -1) {
		vec3 rand_offset = rand3(seed) * 2.0 - 1.0;

		p.pos.xyz = viewPos + rand_offset * K_ENV_QUEUE_RADIUS;

		// Bias precipitation to spawn above camera
		if (selected_style == STYLE_RAIN || selected_style == STYLE_SNOW) {
			p.pos.y = viewPos.y + abs(rand_offset.y) * K_ENV_QUEUE_RADIUS;
		}

		p.pos.w = 10.0 + 5.0 * rand(seed.yx); // Longer lifetime for wrapping

		p.style = selected_style;
		if (selected_style == STYLE_RAIN) {
			p.vel = vec4(0, -40.0, 0, 15.0);
		} else if (selected_style == STYLE_SNOW) {
			p.vel = vec4(0, -3.0, 0, 15.0);
		} else { // STYLE_DUST
			p.vel = vec4(rand3(seed + 0.22) * 0.1, 1.0);
		}

		p.emitter_index = -1;
		p.emitter_id = -2; // Denotes environmental queue
		p.origin.xyz = p.pos.xyz;
		p.origin.w = 2.0;
		return true;
	}
	return false;
}

bool respawnParticle(
	inout Particle p,
	uint           gid,
	int            emitter_index,
	int            num_emitters,
	float          time,
	float          dt,
	float          ambient_density,
	uint           ambient_particle_scale,
	int            num_chunks,
	vec3           viewPos,
	vec3           viewDir,
	float          nightFactor,
	sampler3D      curlTexture,
	sampler2DArray heightmapArray,
	sampler2DArray biomeMap,
	float          cellSize,
	uint           gridSize
) {
	if (p.pos.w <= 0.0) {
		// --- 3a. Fire Effect Respawn ---
		if (emitter_index != -1 && num_emitters > 0 && emitter_index < num_emitters) {
			spawnEmitterParticle(p, gid, emitter_index, time, dt, curlTexture);
			if (p.pos.w > 0.0) return true;
		}

		// --- 3b. Sparse Ambient Respawn ---
		if (p.pos.w <= 0.0 && emitter_index == -1) {
			if (spawnAmbientParticle(
				p,
				gid,
				time,
				ambient_density,
				ambient_particle_scale,
				num_chunks,
				viewPos,
				viewDir,
				nightFactor,
				curlTexture,
				heightmapArray,
				biomeMap,
				cellSize,
				gridSize
			)) return true;
		}

		// --- 3c. Environmental Queue Respawn ---
		if (p.pos.w <= 0.0 && emitter_index == -1) {
			if (spawnEnvironmentalQueue(p, gid, time, ambient_density, ambient_particle_scale, viewPos)) return true;
		}

		if (p.pos.w <= 0.0) {
			p.pos.w = 0.0;
			if (emitter_index == -1) {
				p.style = STYLE_AMBIENT;
				p.emitter_index = -1;
				p.emitter_id = -1;
			}
		}
	}
	return false;
}

void handleVisibility(Particle p, uint gid) {
	if (p.pos.w > 0.0 && isSphereInFrustum(p.pos.xyz, 2.0)) {
		uint idx = atomicAdd(draw_command.count, 1);
		visible_indices[idx] = gid;
	}
}

#endif
