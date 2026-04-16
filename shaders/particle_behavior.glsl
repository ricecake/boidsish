#ifndef PARTICLE_BEHAVIOR_GLSL
#define PARTICLE_BEHAVIOR_GLSL

#include "helpers/spatial_hash.glsl"
#include "particle_helpers.glsl"
#include "particle_types.glsl"

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

// Helper to avoid sign(0) == 0
vec2 signNotZero(vec2 v) {
	return vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
}

// Octahedral decoding for normals (+Y is Up)
vec3 octDecode(vec2 e) {
	e = e * 2.0 - 1.0;
	vec3 v = vec3(e.x, 1.0 - abs(e.x) - abs(e.y), e.y);
	if (v.y < 0.0) {
		v.xz = (1.0 - abs(v.zx)) * signNotZero(v.xz);
	}
	return normalize(v);
}

void handleTerrainCollision(inout Particle p, int num_chunks, sampler2DArray heightmapArray) {
	bool collided = false;
	for (int i = 0; i < num_chunks; i++) {
		ChunkInfo chunk = chunks[i];
		if (p.pos.x >= chunk.worldOffset.x && p.pos.x < chunk.worldOffset.x + chunk.size &&
		    p.pos.z >= chunk.worldOffset.y && p.pos.z < chunk.worldOffset.y + chunk.size) {
			vec2  uv = (p.pos.xz - chunk.worldOffset) / chunk.size;

			// Use high-resolution baked data if available, otherwise fallback to low-res
			vec4 terrain = texture(uBakedHeightNormal, vec3(uv, chunk.slice));
			float height = terrain.r;
			vec3  normal = octDecode(terrain.gb);

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
	int   sub_style = p.emitter_id;

	// Simple repulsion from other particles using the spatial grid
	if (sub_style == 4) { // Skip debug

		float twinkle_t = float(time) - float(p.extras[1]);

		bool is_refractory = (twinkle_t < 0.75);

		if (!is_refractory) {
			p.vel.w += dt / p.extras[0];
		}

		if (!is_refractory) {
			for (int x = -1; x <= 1; x++) {
				for (int y = -1; y <= 1; y++) {
					for (int z = -1; z <= 1; z++) {
						uint cellIdx = get_cell_idx(p.pos.xyz + vec3(x, y, z) * cellSize, cellSize, gridSize);
						int  otherIdx = grid_heads[cellIdx];
						int  safety = 0;
						while (otherIdx != -1 && safety < 100) {
							if (otherIdx != int(gl_GlobalInvocationID.x)) {
								Particle otherP = particles[otherIdx];
								float    other_twinkle_t = float(time) - float(otherP.extras[1]);
								if (other_twinkle_t < (dt * 2.0)) {
									float distSq = pow(distance(otherP.pos.xyz, p.pos.xyz), 2.0);

									float pulse_strength = 0.15;
									p.vel.w += pulse_strength / max(distSq, 0.01);
								}
							}
							otherIdx = grid_next[otherIdx];
							safety++;
						}
					}
				}
			}
		}
		if (p.vel.w >= 1.0 && !is_refractory) {
			p.vel.w = 0.0;
			p.extras[1] = time; // This instantly resets twinkle_t to 0, entering refractory
		}
	}

	if (sub_style == 0 || sub_style == 1) { // Leaf or Petal
		float curlInfluence = 1.2;
		p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
		p.vel.y -= 0.1 * dt; // Slight gravity
		p.vel.xyz *= pow(0.98, dt / 0.016);
	} else if (sub_style == 2) { // Bubble
		float curlInfluence = 0.5;
		p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
		p.vel.y += 0.4 * dt; // Rising
		p.vel.xyz *= pow(0.97, dt / 0.016);
		maxSpeed = 1.5;
	} else if (sub_style == 3) { // Snowflake
		float curlInfluence = 0.3;
		p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
		p.vel.y -= 0.5 * dt; // Falling
		p.vel.xyz *= pow(0.99, dt / 0.016);
		maxSpeed = 1.2;
	} else if (sub_style == 4) { // Firefly
		float curlInfluence = 0.8;
		p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
		p.vel.y += 0.15 * dt; // Gentle rising
		p.vel.xyz *= pow(0.99, dt / 0.016);
	} else {
		p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * dt;
		p.vel.xyz *= pow(0.99, dt / 0.016);
	}

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

	if (length(p.vel.xyz) > maxSpeed) {
		p.vel.xyz = normalize(p.vel.xyz) * maxSpeed;
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
	if (p.style == 0) { // MissileExhaust
		maxSpeed = kExhaustSpeed;
		p.vel.xyz -= p.vel.xyz * kExhaustDrag * 2 * (1 - (p.vel.xyz / 30)) * dt;
	} else if (p.style == 1) { // Explosion
		maxSpeed = kExplosionSpeed;
		p.vel.xyz -= p.vel.xyz * kExplosionDrag * dt;
		float dist = distance(p.pos.xyz, p.epicenter);
		float curlInfluence = smoothstep(5.0, 30.0, dist) + smoothstep(5, 1, length(p.vel.xyz) * kExplosionDrag * dt);
		p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * 15.0 * dt;
	} else if (p.style == 2) { // Fire
		// maxSpeed = kFireSpeed;
		p.vel.y += (kFireSpeed / p.pos.w) * dt;
		p.vel.x += (rand(p.pos.xy + time) - 0.5) * kFireSpread * dt * 0.25;
		p.vel.z += (rand(p.pos.yz + time) - 0.5) * kFireSpread * dt * 0.25;
	} else if (p.style == 3) { // Sparks
		maxSpeed = kSparksSpeed;
		p.vel.xyz -= p.vel.xyz * kSparksDrag * dt;
		p.vel.y -= kSparksGravity * dt;
	} else if (p.style == 4) { // Glitter
		maxSpeed = kGlitterSpeed;
		p.vel.xyz -= p.vel.xyz * kGlitterDrag * dt;
		float dist = distance(p.pos.xyz, p.epicenter);
		float curlInfluence = smoothstep(5.0, 30.0, dist) + smoothstep(5, 1, length(p.vel.xyz) * kGlitterDrag * dt);
		p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * 5.0 * dt;
		p.vel.y -= kGlitterGravity * dt;
	} else if (p.style == 6) { // Bubbles
		float curlInfluence = 0.5;
		p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
		p.vel.y += 0.4 * dt; // Rising
		p.vel.xyz *= 0.97;
		maxSpeed = 1.5;
	} else if (p.style == 7) { // Fireflies
		float curlInfluence = 0.8;
		p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
		p.vel.y += 0.15 * dt; // Gentle rising
		p.vel.xyz *= 0.99;
		maxSpeed = 2.0;
	} else if (p.style == 8) { // Debug
		p.vel.xyz = vec3(0.0);
	} else if (p.style == 9) { // Cinder
		float ageFactor = clamp(1.0 - (p.pos.w / kCinderLifetime), 0.0, 1.0);
		float curlInfluence = ageFactor * kCinderDriftIntensity;
		p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
		p.vel.y += kCinderBuoyancy * ageFactor * dt;
		p.vel.xyz *= 0.98; // Drag
		maxSpeed = 5.0;
	}

	if (p.style != 3 && p.style != 4 && p.style != 5 && p.style != 6 && p.style != 7 && p.style != 8 && p.style != 9 &&
	    p.style != 2) {
		p.vel.y -= 0.05 * dt;
	}

	if (p.style != 5 && p.style != 6 && p.style != 7 && p.style != 8 && p.style != 9 && p.style != 2) {
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
	if (p.style == 5) {
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
	} else {
		updateFireBehavior(p, dt, time, curlTexture, num_chunks, heightmapArray);
	}
}

#endif
