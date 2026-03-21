#ifndef PARTICLE_LIFECYCLE_GLSL
#define PARTICLE_LIFECYCLE_GLSL

#include "frustum.glsl"
#include "particle_behavior.glsl"
#include "particle_helpers.glsl"
#include "particle_types.glsl"

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
		if (emitter.request_clear != 0 || (p.style == 5 && p.pos.w > 0.0) ||
		    (p.emitter_id != emitter.id && p.pos.w <= 0.50)) {
			p.pos.w = 0.0;
		}
	}

	// Aging
	p.pos.w -= dt;

	// View-distance/direction culling
	if (distance(p.pos.xz, viewPos.xz) > 5.0 && dot(p.pos.xz - viewPos.xz, viewDir.xz) < 0) {
		p.pos.w = 0;
	}

	return p.pos.w <= 0.0;
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
	if (p.pos.w <= 0.0) {
		// --- 3a. Fire Effect Respawn ---
		if (emitter_index != -1 && num_emitters > 0 && emitter_index < num_emitters) {
			Emitter emitter = emitters[emitter_index];
			if (emitter.is_active != 0) {
				p.emitter_index = emitter_index;
				p.emitter_id = emitter.id;
				p.style = emitter.style;
				p.epicenter = emitter.position;

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
					if (p.style == 0) { // MissileExhaust
						p.pos = vec4(spawn_pos, kExhaustLifetime * lifeTimeModifier);
						vec3 perpendicular = normalize(cross(emitter.direction, random_vec));
						vec3 spread_dir = mix(emitter.direction, perpendicular, kExhaustSpread * rand(seed.yx));
						p.vel = vec4(emitter.velocity + normalize(spread_dir) * kExhaustSpeed * rand(seed), 0.0);
					} else if (p.style == 1) { // Explosion
						p.pos = vec4(spawn_pos, kExplosionLifetime * lifeTimeModifier);
						p.vel = vec4(
							normalize(random_vec) * kExplosionSpeed * 0.5 +
								abs(fbmCurlMagnitude(p.pos.xyz, dt, curlTexture)),
							0.0
						);
					} else if (p.style == 2) { // Fire
						p.pos = vec4(spawn_pos + random_vec * 0.1, kFireLifetime * lifeTimeModifier);
						p.vel = vec4(0, kFireSpeed, 0, 0);
					} else if (p.style == 3) { // Sparks
						p.pos = vec4(spawn_pos, kSparksLifetime * (rand(seed) * 0.5 + 0.5));
						p.vel = vec4(emitter.velocity + normalize(random_vec) * kSparksSpeed * rand(seed.yx), 0.0);
					} else if (p.style == 4) { // Glitter
						p.pos = vec4(spawn_pos, kGlitterLifetime * lifeTimeModifier);
						vec3 dir = length(emitter.direction) > 0.001 ? normalize(emitter.direction) : vec3(0, 1, 0);
						vec3 spread_dir = normalize(dir + (random_vec * kGlitterSpread));
						p.vel = vec4(emitter.velocity + spread_dir * kGlitterSpeed * (0.5 + 0.5 * rand(seed)), 0.0);
					} else if (p.style == 6) { // Bubbles
						p.pos = vec4(spawn_pos, 3.0 + rand(seed) * 2.0);
						p.vel = vec4(rand3(seed) * 0.2, 0.0);
						p.vel.y += 0.2;
					} else if (p.style == 7) { // Fireflies
						p.pos = vec4(spawn_pos, 5.0 + rand(seed) * 5.0);
						p.vel = vec4(rand3(seed) * 0.1, 0.0);
					} else if (p.style == 8) { // Debug
						p.pos = vec4(spawn_pos, 10.0);
						p.vel = vec4(0.0, 0.0, 0.0, 0.0);
					} else if (p.style == 9) { // Cinder
						p.pos = vec4(spawn_pos, kCinderLifetime * (0.8 + 0.4 * rand(seed)));
						p.vel = vec4(normalize(random_vec) * kCinderSpeed * rand(seed.yx), 0.0);
					}
				}
			}
		}

		if (p.pos.w <= 0.0) {
			// --- 3b. Sparse Ambient Respawn ---
			uint ambient_limit = uint(particles.length() * ambient_density);
			vec2 spawnSeed = vec2(float(gid) * 0.123, time * 0.456);
			if (gid >= (particles.length() - ambient_limit) && num_chunks > 0) {
				uint  particleSeed = hash(gid ^ uint(time * 10.0));
				float r_dist = randomFloat(particleSeed);
				float dist = sqrt(100.0 + r_dist * 249900.0);

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

				if (chunk_idx != -1 && distance(pos.xz, viewPos.xz) <= 750.0) {
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
					pos += 5 * viewDir;

					bool valid_biome = (biome_idx >= 0 && biome_idx <= 4) || biome_idx == 7;
					if (valid_biome) {
						float total_lifetime = 10.0 + rand(spawnSeed + 4.4) * 5.0;
						float skipped_time = rand(spawnSeed + 7.7) * total_lifetime;

						p.style = 5; // Ambient
						p.emitter_index = biome_idx;
						p.pos = vec4(
							pos.x,
							height + 1.0 + rand(spawnSeed + 3.3) * 2.0,
							pos.z,
							total_lifetime - skipped_time
						);
						p.vel = vec4(rand3(spawnSeed + 5.5) * 0.5, 0.0);
						p.epicenter = p.pos.xyz;

						int sub_style = 0;
						if (biome_idx == 7) {
							sub_style = 3; // Snowflake
						} else if (biome_idx == 0) {
							sub_style = 2; // Bubble
						} else {
							if (nightFactor > 0.5) {
								sub_style = 4; // Firefly
								p.extras[0] = 3.0 + 2.0 * fract(randomFloat(hash(particleSeed)));
								p.extras[1] = 0.0;
								p.vel.w = 0.0;
							} else {
								float r = rand(spawnSeed + 6.6);
								if (biome_idx == 4)
									sub_style = (r < 0.7) ? 1 : 0;
								else
									sub_style = (r < 0.2) ? 1 : 0;
							}
						}

						p.emitter_id = sub_style;

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
		}

		if (p.pos.w <= 0.0) {
			p.pos.w = 0.0;
			if (emitter_index == -1) {
				p.style = 5;
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
