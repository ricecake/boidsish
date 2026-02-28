#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

#include "atmosphere/common.glsl"
#include "helpers/lighting.glsl"
#include "helpers/noise.glsl"

uniform mat4 invProjection;
uniform mat4 invView;

uniform sampler2D u_transmittanceLUT;
uniform sampler2D u_skyViewLUT;

uniform vec3 u_sunRadiance; // Added for consistency with scattering

vec3 getTransmittance(float r, float mu) {
	vec2 uv = transmittanceToUV(r, mu);
	return texture(u_transmittanceLUT, uv).rgb;
}

vec3 sampleSkyView(vec3 rd) {
	float elevation = asin(clamp(rd.y, -1.0, 1.0));
	float azimuth = atan(rd.x, -rd.z);
	if (azimuth < 0.0)
		azimuth += 2.0 * PI;

	// Non-linear mapping for better horizon detail
	float v = (elevation < 0.0) ? (0.5 - 0.5 * sqrt(-elevation / (PI * 0.5)))
								: (0.5 + 0.5 * sqrt(elevation / (PI * 0.5)));
	vec2  uv = vec2(azimuth / (2.0 * PI), v);
	return texture(u_skyViewLUT, uv).rgb;
}


float starLayer(vec3 dir) {
	float scale = 100.0;
	vec3  id = floor(dir * scale);
	vec3  local_uv = fract(dir * scale);
	vec3  star_pos = hash33(id);
	float brightness = abs(sin(time / 2 + star_pos.x * 100));
	vec3  center = vec3(0.5) + (star_pos - 0.5) * 0.8;
	float dist = length(local_uv - center);
	float radius = 0.05 * brightness;
	return smoothstep(radius, radius * 0.5, dist);
}

void main() {
	vec4 clip = vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
	vec4 view_ray = invProjection * clip;
	vec3 world_ray = (invView * vec4(view_ray.xy, -1.0, 0.0)).xyz;
	world_ray = normalize(world_ray);

	vec3 sunDir = vec3(0, 1, 0);
	vec3 sunColor = vec3(1);
	if (num_lights > 0) {
		sunDir = normalize(-lights[0].direction);
		sunColor = lights[0].color;
	}

	// 1. Atmospheric Scattering
	vec3 skyRadiance = sampleSkyView(world_ray);

	// 2. Sun Disc with Realistic Distortion (flattening near horizon)
	float cosTheta = dot(world_ray, sunDir);
	float sunAngularRadius = 0.02; // approx 1.0 degrees

	// Construct local sun frame
	vec3 sunUp = vec3(0, 1, 0);
	vec3 sunRight = normalize(cross(sunUp, sunDir));
	sunUp = cross(sunDir, sunRight);

	// Project ray into sun frame
	float rayLocalX = dot(world_ray, sunRight);
	float rayLocalY = dot(world_ray, sunUp);
	float rayLocalZ = dot(world_ray, sunDir);

	// Distort vertical axis near horizon
	float atmosphericRefraction = 1.0 + 0.6 * exp(-max(0.0, sunDir.y * 10.0));
	float flattenedY = rayLocalY * atmosphericRefraction;

	// Effective angle to sun center
	float distSq = rayLocalX * rayLocalX + flattenedY * flattenedY;
	float sunMask = smoothstep(
		sunAngularRadius * sunAngularRadius,
		(sunAngularRadius - 0.001) * (sunAngularRadius - 0.001),
		distSq
	);
	// Ensure we are in front of the sun
	sunMask *= step(0.99, rayLocalZ);

	float r = kEarthRadius + viewPos.y / 1000.0;
	vec3  sunTransmittance = getTransmittance(r, sunDir.y);
	// Use u_sunRadiance if available (via AtmosphereManager) or fallback to simple sunColor
	vec3 radiance = length(u_sunRadiance) > 0.0 ? u_sunRadiance : (sunColor * 20.0);
	vec3 sunDisc = radiance * sunMask * sunTransmittance;

	// 3. Stars and Nebula
	vec3 stars = starLayer(world_ray) * vec3(1.0, 0.9, 0.8);

	vec3  p = world_ray * 4.0;
	vec3  warp_offset = vec3(fbm(p + time * 0.05));
	float nebula_noise = fbm(p + warp_offset * 0.5);
	vec3  nebula = mix(vec3(0.0, 0.1, 0.4), vec3(0.8, 0.2, 0.7), nebula_noise) * 0.4;

	vec3 skyTransmittance = getTransmittance(r, world_ray.y);
	vec3 spaceBackground = (stars + nebula) * skyTransmittance;

	vec3 finalColor = skyRadiance + sunDisc + spaceBackground;

	FragColor = vec4(finalColor, 1.0);
}
