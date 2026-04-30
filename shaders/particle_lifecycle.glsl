#ifndef PARTICLE_LIFECYCLE_GLSL
#define PARTICLE_LIFECYCLE_GLSL

#include "frustum.glsl"
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
	// Hijack/Clear Logic
	if (emitter_index != -1 && num_emitters > 0 && emitter_index < num_emitters) {
		Emitter emitter = emitters[emitter_index];
		if (emitter.request_clear != 0 || (p.emitter_id < 0 && p.pos.w > 0.0) ||
		    (p.emitter_id != emitter.id && p.pos.w <= 0.50)) {
			p.pos.w = 0.0;
		}
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

void spawnAmbientParticle(
	inout Particle p,
	uint           gid,
	float          time,
	float          ambient_density,
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
	uint ambient_limit = uint(particles.length() * ambient_density);
	vec2 spawnSeed = vec2(float(gid) * 0.123, time * 0.456);
	if (gid < (particles.length() - ambient_limit) || num_chunks <= 0) return;

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
			float total_lifetime = 10.0 + rand(spawnSeed + 4.4) * 5.0;
			float skipped_time = rand(spawnSeed + 7.7) * total_lifetime;

			p.emitter_id = -1;
			p.emitter_index = biome_idx;
			p.pos = vec4(
				pos.x,
				height + 1.0 + rand(spawnSeed + 3.3) * 2.0,
				pos.z,
				total_lifetime - skipped_time
			);
			p.vel = vec4(rand3(spawnSeed + 5.5) * 0.5, 15.0);
			p.origin.xyz = p.pos.xyz;
			p.origin.w = 0.0; // Last twinkle time

			if (biome_idx == 7) {
				p.style = STYLE_SNOW;
				if (rand(spawnSeed + 6.6) > weight_snow) {
					p.pos.w = 0.0;
					return;
				}
			} else if (biome_idx == 0) {
				p.style = STYLE_BUBBLES;
				if (rand(spawnSeed + 6.6) > weight_bubble) {
					p.pos.w = 0.0;
					return;
				}
			} else {
				if (nightFactor > 0.5) {
					p.style = STYLE_FIREFLIES;
					p.phase = 3.0 + 2.0 * fract(randomFloat(hash(particleSeed)));
					p.counter = 0.0;

				// Weight-based rejection
				if (rand(spawnSeed + 6.6) > weight_firefly) {
					p.pos.w = 0.0;
					return;
				}
				} else {
					float r = rand(spawnSeed + 6.6);
					if (r < 0.15 && (biome_idx == 1 || biome_idx == 3 || biome_idx == 4)) {
						p.style = STYLE_BIRDS;
						p.phase = rand(spawnSeed + 8.8) * 6.28;
						p.pos.y = height + 0.1;
						p.pos.w = 300;
						p.vel.w = 45.0; // Bird size

					// Weight-based rejection
					if (rand(spawnSeed + 6.6) > weight_bird) {
						p.pos.w = 0.0;
						return;
					}
					} else {
					// Apply weighted style selection
					float w_petal = (biome_idx == 4) ? 0.7 : 0.2;
					w_petal *= weight_petal;
					float w_leaf = (1.0 - ((biome_idx == 4) ? 0.7 : 0.2)) * weight_leaf;

					float total_w = w_petal + w_leaf;
					if (total_w > 0.0) {
						float rand_w = rand(spawnSeed + 9.9) * total_w;
						if (rand_w < w_petal) {
							p.style = STYLE_PETAL;
						} else {
							p.style = STYLE_LEAF;
						}
					} else {
						p.pos.w = 0.0;
						return;
					}
					}
				}
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
		}
	}
}

void spawnPrecipitation(inout Particle p, uint gid, float time, vec3 viewPos) {
	vec2 seed = vec2(float(gid) * 0.123, time * 0.456);
	float spawn_chance = max(rain_intensity, snow_intensity) * 0.6;

	if (rand(seed + 0.77) < spawn_chance) {
		vec3 rand_offset = rand3(seed) * 2.0 - 1.0;
		float box_w = 100.0;
		float box_h = 50.0;

		p.pos.x = viewPos.x + rand_offset.x * box_w;
		p.pos.z = viewPos.z + rand_offset.z * box_w;
		p.pos.y = viewPos.y + (rand_offset.y * 0.5 + 0.5) * box_h;
		p.pos.w = 2.0 + 1.0 * rand(seed.yx);

		if (rain_intensity > snow_intensity) {
			p.style = STYLE_RAIN;
			p.vel = vec4(0, -40.0, 0, 15.0);
		} else {
			p.style = STYLE_SNOW;
			p.vel = vec4(0, -3.0, 0, 15.0);
		}
		p.emitter_index = -1;
		p.emitter_id = -2; // Denotes precipitation
		p.origin.xyz = p.pos.xyz;
		p.origin.w = 1.0;
	}
}

void respawnParticle(
	inout Particle p,
	uint           gid,
	int            emitter_index,
	int            num_emitters,
	float          time,
	float          dt,
	float          ambient_density,
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
	if (particles_enabled == 0) return;

	if (p.pos.w <= 0.0) {
		// --- 3a. Fire Effect Respawn ---
		if (emitter_index != -1 && num_emitters > 0 && emitter_index < num_emitters) {
			spawnEmitterParticle(p, gid, emitter_index, time, dt, curlTexture);
		}

		// --- 3b. Sparse Ambient Respawn ---
		if (p.pos.w <= 0.0) {
			spawnAmbientParticle(
				p,
				gid,
				time,
				ambient_density,
				num_chunks,
				viewPos,
				viewDir,
				nightFactor,
				curlTexture,
				heightmapArray,
				biomeMap,
				cellSize,
				gridSize
			);
		}

		// --- 3c. Precipitation Respawn ---
		if (p.pos.w <= 0.0 && (rain_intensity > 0.01 || snow_intensity > 0.01)) {
			spawnPrecipitation(p, gid, time, viewPos);
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
}

void handleVisibility(Particle p, uint gid) {
	if (p.pos.w > 0.0 && isSphereInFrustum(p.pos.xyz, 2.0)) {
		uint idx = atomicAdd(draw_command.count, 1);
		visible_indices[idx] = gid;
	}
}

#endif
