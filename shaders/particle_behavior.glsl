#ifndef PARTICLE_BEHAVIOR_GLSL
#define PARTICLE_BEHAVIOR_GLSL

#include "helpers/spatial_hash.glsl"
#include "particle_helpers.glsl"
#include "particle_types.glsl"
#include "visual_effects.glsl"
#include "helpers/wind.glsl"
#include "helpers/noise.glsl"

// Simulation parameters
const float kExhaustSpeed = 30.0;
const float kExhaustSpread = 0.1;
const float kExhaustLifetime = 2.0;
const float kExhaustDrag = 1.5;

const float kExplosionSpeed = 30.0;
const float kExplosionLifetime = 2.5;
const float kExplosionDrag = 2.0;

const float kFireSpeed = 0.5;
const float kFireSpread = 4.2;
const float kFireLifetime = 5.0;

const float kSparksSpeed = 40.0;
const float kSparksLifetime = 0.8;
const float kSparksDrag = 1.0;
const float kSparksGravity = 15.0;

const float kGlitterSpeed = 25.0;
const float kGlitterSpread = 0.4;
const float kGlitterLifetime = 3.5;
const float kGlitterDrag = 1.2;
const float kGlitterGravity = 4.0;

const float kCinderSpeed = 1.0;
const float kCinderLifetime = 8.0;
const float kCinderDriftIntensity = 4.0;
const float kCinderBuoyancy = 1.5;

void handleTerrainCollision(inout Particle p, int num_chunks, sampler2DArray heightmapArray) {
	bool collided = false;
	for (int i = 0; i < num_chunks; i++) {
		ChunkInfo chunk = chunks[i];
		if (p.pos.x >= chunk.worldOffset.x && p.pos.x < chunk.worldOffset.x + chunk.size &&
		    p.pos.z >= chunk.worldOffset.y && p.pos.z < chunk.worldOffset.y + chunk.size) {
			vec2  uv = (p.pos.xz - chunk.worldOffset) / chunk.size;
			vec4  terrain = texture(heightmapArray, vec3(uv, chunk.slice));
			float height = terrain.r;
			vec3  normal = vec3(terrain.g, terrain.b, terrain.a);

			if (p.pos.y < height) {
				p.pos.y = height + 0.05;
				p.vel.xyz = reflect(p.vel.xyz, normal) * 0.4;
				collided = true;
			}
			break;
		}
	}

	if (!collided && p.pos.y <= 0.0) {
		p.pos.y = 0.1;
		p.vel.y *= -0.25;
		p.vel.xz *= 0.8;
	}
}

void applyAmbientAvoidance(inout Particle p, float dt, float time, vec3 viewPos, vec3 viewDir, sampler3D curlTexture) {
	float mildRepulsionStrength = 5.0;
	float capsuleWidth = 50.0;
	float noiseWeight = 0.5;
	float pushStrength = 20.0;

	vec3  relativePos = p.pos.xyz - viewPos;
	float distToCam = length(relativePos);
	vec3  camToParticle = distToCam > 0.001 ? relativePos / distToCam : vec3(0.0, 1.0, 0.0);
	float projection = dot(relativePos, viewDir);
	vec3  rejectDir = relativePos - (projection * viewDir);
	float distToAxis = length(rejectDir);
	float sphereInfluence = smoothstep(20.0, 10.0, distToCam);
	float inwardSpeed = dot(p.vel.xyz, -camToParticle);
	float isFront = step(0.0, projection);
	float forwardFalloff = smoothstep(150.0, 0.0, distToCam);
	float axisFalloff = smoothstep(capsuleWidth, 0.0, distToAxis);
	float directionBlend = clamp(distToCam / 150.0, 0.0, 1.0);
	vec3  rejectNorm = distToAxis > 0.001 ? rejectDir / distToAxis : vec3(0.0, 1.0, 0.0);
	vec3  flatReject = vec3(rejectNorm.x, 0.0, rejectNorm.z);
	vec3  lateralPush = length(flatReject) > 0.001 ? normalize(flatReject) : vec3(1.0, 0.0, 0.0);
	vec3  terrainSafeReject = normalize(
		mix(rejectNorm, lateralPush + vec3(0, -rejectNorm.y, 0), smoothstep(0.25, -1, rejectNorm.y))
	);
	vec3 basePushDir = normalize(mix(terrainSafeReject, camToParticle, directionBlend));
	vec3 finalPushDir = normalize(basePushDir + curlNoise(p.pos.xyz, time, curlTexture) * noiseWeight);

	p.vel.xyz += step(0, inwardSpeed) * (camToParticle)*inwardSpeed * sphereInfluence;
	p.vel.xyz += camToParticle * mildRepulsionStrength * sphereInfluence;
	p.vel.xyz += finalPushDir * pushStrength * forwardFalloff * axisFalloff * isFront;
}

void updateRocketTrail(inout Particle p, float dt) {
	p.vel.xyz -= p.vel.xyz * kExhaustDrag * 2.0 * (1.0 - (length(p.vel.xyz) / 30.0)) * dt;
	p.color = vec4(0.1, 0.1, 0.1, p.pos.w * 0.4);
	p.vel.w = smoothstep(0.0, 1.0, p.pos.w / kExhaustLifetime) * 15.0;
	p.origin.w = 2.0; // Intensity
}

void updateExplosion(inout Particle p, float dt, float time, sampler3D curlTexture) {
	p.vel.xyz -= p.vel.xyz * kExplosionDrag * dt;
	float dist = distance(p.pos.xyz, p.origin.xyz);
	float curlInfluence = smoothstep(5.0, 30.0, dist) + smoothstep(5, 1, length(p.vel.xyz) * kExplosionDrag * dt);
	p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * 15.0 * dt;

	float normLife = clamp(p.pos.w / kExplosionLifetime, 0.0, 1.0);
	p.vel.w = (1.0 - (1.0 - normLife) * (1.0 - normLife)) * 60.0;
	p.color = vec4(1.0, 0.9, 0.5, normLife);
	p.origin.w = 5.0 * normLife;
}

void updateFire(inout Particle p, float dt, float time) {
	p.vel.y += (kFireSpeed / p.pos.w) * dt;
	p.vel.x += (rand(p.pos.xy + time) - 0.5) * kFireSpread * dt * 0.25;
	p.vel.z += (rand(p.pos.yz + time) - 0.5) * kFireSpread * dt * 0.25;

	float normLife = clamp(p.pos.w / kFireLifetime, 0.0, 1.0);
	p.vel.w = smoothstep(2.0 * (1.0 - normLife), normLife, normLife / 2.5) * 25.0;
	p.color = vec4(1.0, 0.6, 0.2, normLife);
	p.origin.w = 2.0 * normLife;
}

void updateSparks(inout Particle p, float dt) {
	p.vel.xyz -= p.vel.xyz * kSparksDrag * dt;
	p.vel.y -= kSparksGravity * dt;

	vec3 hot_color = vec3(1.0, 1.0, 1.0);
	vec3 mid_color = vec3(1.0, 0.8, 0.3);
	p.color.rgb = mix(mid_color, hot_color, smoothstep(0.0, 0.5, p.pos.w));
	float pop = sin(p.pos.w * 600.0);
	p.color.rgb *= (pop > 0.0) ? 3.0 : 0.3;
	p.color.a = smoothstep(0.0, 0.1, p.pos.w);

	p.vel.w = 4.0 + p.pos.w * 20.0;
	p.origin.w = 1.0 * p.color.a;
}

void updateGlitter(inout Particle p, float dt, float time, sampler3D curlTexture) {
	p.vel.xyz -= p.vel.xyz * kGlitterDrag * dt;
	float dist = distance(p.pos.xyz, p.origin.xyz);
	float curlInfluence = smoothstep(5.0, 30.0, dist) + smoothstep(5, 1, length(p.vel.xyz) * kGlitterDrag * dt);
	p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * 5.0 * dt;
	p.vel.y -= kGlitterGravity * dt;

	float hue = time * 2.0 + p.pos.w * 1.5 + float(gl_GlobalInvocationID.x) * 0.1;
	p.color.rgb = 0.6 + 0.4 * cos(hue + vec3(0, 2, 4));
	float twinkle = sin(time * 15.0 + p.pos.w * 5.0) * 0.5 + 0.5;
	p.color.rgb *= 0.6 + 0.4 * twinkle;
	p.color.rgb += vec3(pow(twinkle, 10.0) * 2.0);
	p.color.a = clamp(p.pos.w, 0.0, 1.0);

	p.vel.w = 6.0;
	p.origin.w = 0.5 * p.color.a;
}

void updateBubbles(inout Particle p, float dt, float time, sampler3D curlTexture) {
	float curlInfluence = 0.5;
	p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
	p.vel.y += 0.4 * dt;
	p.vel.xyz *= 0.97;
	p.color = vec4(1.0, 1.0, 1.0, 0.6 * smoothstep(0.0, 0.5, p.pos.w));
	p.vel.w = 15.0;
	p.origin.w = 0.0; // Non-emissive
}

void updateFireflies(inout Particle p, float dt, float time, sampler3D curlTexture) {
	float curlInfluence = 0.8;
	p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
	p.vel.y += 0.15 * dt;
	p.vel.xyz *= 0.99;

	vec3 firefly_base = vec3(0.7, 0.9, 0.1);
	float twinkle = sin(time * 6.0 + float(gl_GlobalInvocationID.x)) * 0.5 + 0.5;
	p.color.rgb = firefly_base * (2.0 + twinkle * 8.0);
	p.color.a = (0.4 + twinkle * 0.6) * smoothstep(0.0, 0.5, p.pos.w);
	p.vel.w = 15.0;
	p.origin.w = 0.2 * p.color.a;
}

void updateCinder(inout Particle p, float dt, float time, sampler3D curlTexture) {
	float ageFactor = clamp(1.0 - (p.pos.w / kCinderLifetime), 0.0, 1.0);
	float curlInfluence = ageFactor * kCinderDriftIntensity;
	p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
	p.vel.y += kCinderBuoyancy * ageFactor * dt;
	p.vel.xyz *= 0.98;

	float cNoise = snoise3d(p.pos.xyz * 15.0 + time * 0.2);
	p.color.rgb = mix(vec3(0.1), vec3(0.25), cNoise * 0.5 + 0.5);
	float highlights = smoothstep(0.4, 0.9, snoise3d(p.pos.xyz * 40.0 + time));
	p.color.rgb = mix(p.color.rgb, vec3(2.5, 0.8, 0.2), highlights);
	p.color.a = smoothstep(0.0, 0.5, p.pos.w);
	p.vel.w = 12.0;
	p.origin.w = 0.5 * p.color.a;
}

void updateRain(inout Particle p, float dt, float time) {
	p.vel.y -= 50.0 * dt;
	vec3 wind = getWindAtPosition(p.pos.xyz);
	p.vel.xyz += wind * 5.0 * dt;
	p.vel.xyz *= 0.99;
	p.color = vec4(0.7, 0.8, 1.0, 1.0) * 2.0;
	p.color.a *= smoothstep(0.0, 0.1, p.pos.w);
	p.vel.w = 100.0;
	p.origin.w = 0.0;
}

void updateSnow(inout Particle p, float dt, float time) {
	p.vel.y -= 2.0 * dt;
	vec3 wind = getWindAtPosition(p.pos.xyz);
	p.vel.xyz += wind * 10.0 * dt;
	p.vel.x += sin(time * 5.0 + float(gl_GlobalInvocationID.x)) * 0.5 * dt;
	p.vel.z += cos(time * 4.0 + float(gl_GlobalInvocationID.x)) * 0.5 * dt;
	p.vel.xyz *= 0.95;
	p.color = vec4(1.0, 1.0, 1.0, 1.0) * 1.5;
	p.color.a *= smoothstep(0.0, 0.1, p.pos.w);
	p.vel.w = 80.0;
	p.origin.w = 0.0;
}

void updateLeaf(inout Particle p, float dt, float time, sampler3D curlTexture) {
	float curlInfluence = 1.2;
	p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
	p.vel.y -= 0.1 * dt;
	p.vel.xyz *= pow(0.98, dt / 0.016);

	vec3 leaf_green = vec3(0.2, 0.4, 0.1);
	vec3 leaf_brown = vec3(0.4, 0.3, 0.15);
	p.color.rgb = mix(leaf_green, leaf_brown, sin(p.pos.x * 0.1 + p.pos.z * 0.1) * 0.5 + 0.5);
	float flutter = sin(time * 5.0 + p.pos.x + p.pos.y) * 0.3 + 0.7;
	p.color.rgb *= flutter;
	p.color.rgb += vec3(0.1) * pow(flutter, 10.0);
	p.color.a = smoothstep(0.0, 0.5, p.pos.w) * 0.9;
	p.vel.w = 15.0;
	p.origin.w = 0.0;
}

void updatePetal(inout Particle p, float dt, float time, sampler3D curlTexture) {
	float curlInfluence = 1.2;
	p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
	p.vel.y -= 0.1 * dt;
	p.vel.xyz *= pow(0.98, dt / 0.016);

	p.color.rgb = vec3(1.0, 0.5, 0.8);
	float flutter = sin(time * 8.0 + p.pos.x * 2.0) * 0.4 + 0.6;
	p.color.rgb *= flutter;
	p.color.a = smoothstep(0.0, 0.5, p.pos.w) * 0.95;
	p.vel.w = 15.0;
	p.origin.w = 0.0;
}

void updateAmbientBubble(inout Particle p, float dt, float time, sampler3D curlTexture) {
	float curlInfluence = 0.5;
	p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
	p.vel.y += 0.4 * dt;
	p.vel.xyz *= pow(0.97, dt / 0.016);
	p.color = vec4(1.0, 1.0, 1.0, 0.6 * smoothstep(0.0, 0.5, p.pos.w));
	p.vel.w = 15.0;
	p.origin.w = 0.0;
}

void updateAmbientSnowflake(inout Particle p, float dt, float time, sampler3D curlTexture) {
	float curlInfluence = 0.3;
	p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
	p.vel.y -= 0.5 * dt;
	p.vel.xyz *= pow(0.99, dt / 0.016);
	p.color = vec4(0.9, 0.95, 1.0, 0.8 * smoothstep(0.0, 0.5, p.pos.w)) * (1.2 + 0.3 * sin(time * 2.0 + p.pos.x));
	p.vel.w = 15.0;
	p.origin.w = 0.0;
}

void updateAmbientFirefly(
	inout Particle p,
	float          dt,
	float          time,
	float          cellSize,
	uint           gridSize,
	sampler3D      curlTexture
) {
	// Sync twinkle state using counter and phase
	p.counter += dt;
	float cycle_time = p.phase + 0.75; // Period + refractory period
	if (p.counter > cycle_time) {
		p.counter = fract(p.counter / cycle_time) * cycle_time;
	}

	// Simple repulsion from other particles using the spatial grid
	if (p.counter > 0.75) { // Only check for sync during "flash ready" phase
		for (int x = -1; x <= 1; x++) {
			for (int y = -1; y <= 1; y++) {
				for (int z = -1; z <= 1; z++) {
					uint cellIdx = get_cell_idx(p.pos.xyz + vec3(x, y, z) * cellSize, cellSize, gridSize);
					int  otherIdx = grid_heads[cellIdx];
					int  safety = 0;
					while (otherIdx != -1 && safety < 100) {
						if (otherIdx != int(gl_GlobalInvocationID.x)) {
							Particle otherP = particles[otherIdx];
							// If neighbor is flashing, speed up our own counter to sync
							if (otherP.counter < 0.6) {
								float distSq = pow(distance(otherP.pos.xyz, p.pos.xyz), 2.0);
								float pulse_strength = 0.15;
								p.counter += (pulse_strength / max(distSq, 0.01)) * dt;
							}
						}
						otherIdx = grid_next[otherIdx];
						safety++;
					}
				}
			}
		}
	}

	float curlInfluence = 0.8;
	p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
	p.vel.y += 0.15 * dt;
	p.vel.xyz *= pow(0.99, dt / 0.016);

	vec3 firefly_base = vec3(0.7, 0.9, 0.1);
	float twinkle = pow(smoothstep(0.0, 0.3, p.counter) * (1.0 - smoothstep(0.4, 0.6, p.counter)), 2) * step(p.counter, 0.6);
	p.color.rgb = firefly_base * (2.0 + twinkle * 8.0);
	p.color.a = 0.01 + step(p.counter, 0.6) * (0.4 + twinkle * 0.6) * smoothstep(0.0, 0.5, p.pos.w);
	p.vel.w = 15.0;
	p.origin.w = 0.5 * p.color.a;
}

void updateAmbientParticle(
	inout Particle p,
	float          dt,
	float          time,
	vec3           viewPos,
	vec3           viewDir,
	float          cellSize,
	uint           gridSize,
	sampler3D      curlTexture,
	int            num_chunks,
	sampler2DArray heightmapArray
) {
	float maxSpeed = 2.0;

	if (p.style == STYLE_LEAF) {
		updateLeaf(p, dt, time, curlTexture);
	} else if (p.style == STYLE_PETAL) {
		updatePetal(p, dt, time, curlTexture);
	} else if (p.style == STYLE_BUBBLES) {
		updateAmbientBubble(p, dt, time, curlTexture);
		maxSpeed = 1.5;
	} else if (p.style == STYLE_SNOW) {
		updateAmbientSnowflake(p, dt, time, curlTexture);
		maxSpeed = 1.2;
	} else if (p.style == STYLE_FIREFLIES) {
		updateAmbientFirefly(p, dt, time, cellSize, gridSize, curlTexture);
	} else {
		p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * dt;
		p.vel.xyz *= pow(0.99, dt / 0.016);
		p.vel.w = 15.0;
		p.color.a = smoothstep(0.0, 0.5, p.pos.w);
		p.origin.w = 0.0;
	}

	applyAmbientAvoidance(p, dt, time, viewPos, viewDir, curlTexture);

	if (length(p.vel.xyz) > maxSpeed) {
		p.vel.xyz = normalize(p.vel.xyz) * maxSpeed;
	}

	p.pos.xyz += p.vel.xyz * dt;
	handleTerrainCollision(p, num_chunks, heightmapArray);
}

void updatePrecipitationBehavior(
	inout Particle p,
	float          dt,
	float          time,
	int            num_chunks,
	sampler2DArray heightmapArray
) {
	if (p.style == STYLE_RAIN) {
		updateRain(p, dt, time);
	} else if (p.style == STYLE_SNOW) {
		updateSnow(p, dt, time);
	}

	p.pos.xyz += p.vel.xyz * dt;
	handleTerrainCollision(p, num_chunks, heightmapArray);
}

void updateFireBehavior(
	inout Particle p,
	float          dt,
	float          time,
	sampler3D      curlTexture,
	int            num_chunks,
	sampler2DArray heightmapArray
) {
	float maxSpeed = 10.0;
	if (p.style == STYLE_ROCKET_TRAIL) {
		updateRocketTrail(p, dt);
		maxSpeed = kExhaustSpeed;
	} else if (p.style == STYLE_EXPLOSION) {
		updateExplosion(p, dt, time, curlTexture);
		maxSpeed = kExplosionSpeed;
	} else if (p.style == STYLE_FIRE) {
		updateFire(p, dt, time);
	} else if (p.style == STYLE_SPARKS) {
		updateSparks(p, dt);
		maxSpeed = kSparksSpeed;
	} else if (p.style == STYLE_GLITTER) {
		updateGlitter(p, dt, time, curlTexture);
		maxSpeed = kGlitterSpeed;
	} else if (p.style == STYLE_BUBBLES) {
		updateBubbles(p, dt, time, curlTexture);
		maxSpeed = 1.5;
	} else if (p.style == STYLE_FIREFLIES) {
		updateFireflies(p, dt, time, curlTexture);
		maxSpeed = 2.0;
	} else if (p.style == STYLE_CINDER) {
		updateCinder(p, dt, time, curlTexture);
		maxSpeed = 5.0;
	} else if (p.style == STYLE_RAIN) {
		updateRain(p, dt, time);
		maxSpeed = 40.0;
	} else if (p.style == STYLE_SNOW) {
		updateSnow(p, dt, time);
		maxSpeed = 5.0;
	}

	if (p.style != STYLE_SPARKS && p.style != STYLE_GLITTER && p.style != STYLE_AMBIENT && p.style != STYLE_BUBBLES && p.style != STYLE_FIREFLIES && p.style != STYLE_DEBUG && p.style != STYLE_CINDER &&
	    p.style != STYLE_FIRE && p.style != STYLE_RAIN && p.style != STYLE_SNOW) {
		p.vel.y -= 0.05 * dt;
	}

	if (p.style != STYLE_AMBIENT && p.style != STYLE_BUBBLES && p.style != STYLE_FIREFLIES && p.style != STYLE_DEBUG && p.style != STYLE_CINDER && p.style != STYLE_FIRE && p.style != STYLE_RAIN && p.style != STYLE_SNOW) {
		p.vel.xyz += vec3(mix(curlNoise(p.pos.xyz, time, curlTexture) * 3, vec3(0, 0, 0), length(p.vel) / maxSpeed)) *
			dt;
	}

	p.pos.xyz += p.vel.xyz * dt;
	handleTerrainCollision(p, num_chunks, heightmapArray);
}

void updateBehavior(
	inout Particle p,
	float          dt,
	float          time,
	vec3           viewPos,
	vec3           viewDir,
	float          cellSize,
	uint           gridSize,
	sampler3D      curlTexture,
	int            num_chunks,
	sampler2DArray heightmapArray
) {
	if (p.emitter_id == -1) {
		updateAmbientParticle(
			p,
			dt,
			time,
			viewPos,
			viewDir,
			cellSize,
			gridSize,
			curlTexture,
			num_chunks,
			heightmapArray
		);
	} else if (p.emitter_id == -2) {
		updatePrecipitationBehavior(p, dt, time, num_chunks, heightmapArray);
	} else {
		updateFireBehavior(p, dt, time, curlTexture, num_chunks, heightmapArray);
	}
}

#endif
