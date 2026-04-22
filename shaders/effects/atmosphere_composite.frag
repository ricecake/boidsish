#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;
uniform sampler2D cloudTexture; // Low-res clouds (temporally accumulated)

uniform mat4 invView;
uniform mat4 invProjection;

uniform float hazeDensity;
uniform float hazeHeight;
uniform vec3  hazeColor;

uniform vec2 cloudTexelSize; // 1.0 / lowResSize

// u_transmittanceLUT is declared in helpers/lighting.glsl
uniform sampler3D u_aerialPerspectiveLUT;

layout(std140, binding = [[VOLUMETRIC_LIGHTING_BINDING]]) uniform VolumetricLighting {
    mat4 invViewProj;
    mat4 prevViewProj;
    vec4 gridParams;     // x: near, y: far, z: log bias, w: cascade count
    vec4 resolution;     // x: gridW, y: gridH, z: gridD, w: intensity
    vec4 sunDir;         // xyz: dir, w: sun intensity
    vec4 sunColor;       // rgb: color, w: mie anisotropy (g)
    vec4 moonDir;        // xyz: dir, w: moon intensity
    vec4 moonColor;      // rgb: color, w: reserved
    vec4 hazeParams;     // x: haze density, y: haze height, z: noise scale, w: noise strength
    vec4 hazeColor;      // rgb: color, w: reserved
    vec4 ambientColor;   // rgb: color, w: scattering scale
    vec4 cloudParams;    // x: cloud coverage, y: cloud density, z: cloud shadow intensity, w: reserved
    vec4 cascadeSplits;
    vec4 viewPosVol;     // xyz: camPos, w: worldScale
    vec4 timeParams;     // x: totalTime, y: frameIndex, zw: reserved
} u_vol;

uniform sampler3D u_volumetricIntegrated[4];

#include "../atmosphere/common.glsl"
#include "../helpers/lighting.glsl"
#include "helpers/math.glsl"

vec3 sampleAerialPerspective(vec3 rd, float distKM) {
	float azimuth = atan(rd.x, -rd.z);
	if (azimuth < 0.0)
		azimuth += 2.0 * PI;
	float elevation = asin(clamp(rd.y, -1.0, 1.0));

	float u = azimuth / (2.0 * PI);
	float v = elevation / PI + 0.5;
	float w = distKM / 32.0; // maxDist in AP LUT

	return texture(u_aerialPerspectiveLUT, vec3(u, v, w)).rgb;
}

float sampleAerialPerspectiveTransmittance(vec3 rd, float distKM) {
	float azimuth = atan(rd.x, -rd.z);
	if (azimuth < 0.0)
		azimuth += 2.0 * PI;
	float elevation = asin(clamp(rd.y, -1.0, 1.0));

	float u = azimuth / (2.0 * PI);
	float v = elevation / PI + 0.5;
	float w = distKM / 32.0;

	return texture(u_aerialPerspectiveLUT, vec3(u, v, w)).a;
}

void main() {
	float depth = texture(depthTexture, TexCoords).r;
	vec3  sceneColor = texture(sceneTexture, TexCoords).rgb;

	float z = depth * 2.0 - 1.0;
	vec4  clipSpacePosition = vec4(TexCoords * 2.0 - 1.0, z, 1.0);
	vec4  viewSpacePosition = invProjection * clipSpacePosition;
	viewSpacePosition /= viewSpacePosition.w;
	vec3 worldPos = (invView * viewSpacePosition).xyz;

	vec3  rayDir = normalize(worldPos - viewPos);
	float dist = length(worldPos - viewPos);

	if (depth >= 0.99999) {
		dist = 50000.0 * worldScale;
	}

	// 1. Bilateral upsample of low-res clouds
	// Weight nearby low-res texels by depth similarity to avoid bleeding across edges
	float centerDepth = dist;
	vec4  cloudData = vec4(0.0);
	float totalWeight = 0.0;

	// Sample a 2x2 neighborhood of the nearest low-res texels
	vec2 lowResUV = TexCoords / cloudTexelSize - 0.5;
	vec2 baseTexel = floor(lowResUV);
	vec2 frac_ = lowResUV - baseTexel;

	for (int dy = 0; dy <= 1; dy++) {
		for (int dx = 0; dx <= 1; dx++) {
			vec2 sampleUV = (baseTexel + vec2(dx, dy) + 0.5) * cloudTexelSize;
			sampleUV = clamp(sampleUV, cloudTexelSize * 0.5, 1.0 - cloudTexelSize * 0.5);

			// Reconstruct depth at this low-res texel center
			float sampleDepthRaw = texture(depthTexture, sampleUV).r;
			float sampleDist;
			if (sampleDepthRaw >= 1.0) {
				sampleDist = 50000.0 * worldScale;
			} else {
				float sz = sampleDepthRaw * 2.0 - 1.0;
				vec4  sClip = vec4(sampleUV * 2.0 - 1.0, sz, 1.0);
				vec4  sView = invProjection * sClip;
				sView /= sView.w;
				vec3 sWorld = (invView * sView).xyz;
				sampleDist = length(sWorld - viewPos);
			}

			// Bilinear weight
			float bx = (dx == 0) ? (1.0 - frac_.x) : frac_.x;
			float by = (dy == 0) ? (1.0 - frac_.y) : frac_.y;
			float spatialW = bx * by;

			// Depth similarity weight — exponential falloff
			// Balanced for high-dynamic range volumetric scattering
			float depthDiff = abs(centerDepth - sampleDist) / max(centerDepth, 1.0);
			float depthW = exp(-depthDiff * 20.0);

			float w = spatialW * depthW;
			cloudData += texture(cloudTexture, sampleUV) * w;
			totalWeight += w;
		}
	}
	cloudData /= max(totalWeight, 1e-6);

	vec3  cloudColor = cloudData.rgb;
	float cloudTransmittance = cloudData.a;

	// 2. High-res Atmosphere (Haze)
	float distKM = (dist / 1000.0) * hazeDensity;
	vec3  inScattering = sampleAerialPerspective(rayDir, distKM);
	float transmittance = sampleAerialPerspectiveTransmittance(rayDir, distKM);

	// 3. Cloud Atmospheric Integration
	// Clouds should also be affected by the atmosphere between them and the camera.
	float cloudDist = (cloudAltitude * worldScale - viewPos.y) / max(abs(rayDir.y), 0.01);
	cloudDist = clamp(cloudDist, 0.0, dist);
	float cloudDistKM = (cloudDist / 1000.0) * hazeDensity;

	vec3  atmosInScattering = sampleAerialPerspective(rayDir, cloudDistKM);
	float atmosTransmittance = sampleAerialPerspectiveTransmittance(rayDir, cloudDistKM);

	// Combine everything
	// Colossal objects write depth ~0.99999 — treat them like sky (no aerial perspective
	// fog, which would completely wash them out at that reconstructed distance)
	bool isSky = depth >= 0.99999;

	// 4. Volumetric Lighting Integration (Cascaded Sampling with Blending)
	int volCascade = 3;
    for (int i = 0; i < 4; ++i) {
        if (dist < u_vol.cascadeSplits[i]) {
            volCascade = i;
            break;
        }
    }

	float v_near = (volCascade == 0) ? u_vol.gridParams.x : u_vol.cascadeSplits[volCascade-1];
    float v_far = u_vol.cascadeSplits[volCascade];

	float volZ = log(max(dist, v_near) / v_near) / log(v_far / v_near);
	vec4 volumetric;
    if (volCascade == 0) volumetric = texture(u_volumetricIntegrated[0], vec3(TexCoords, volZ));
    else if (volCascade == 1) volumetric = texture(u_volumetricIntegrated[1], vec3(TexCoords, volZ));
    else if (volCascade == 2) volumetric = texture(u_volumetricIntegrated[2], vec3(TexCoords, volZ));
    else volumetric = texture(u_volumetricIntegrated[3], vec3(TexCoords, volZ));

    // Smooth cascade transitions
    if (volCascade < 3) {
        float blendStart = u_vol.cascadeSplits[volCascade] * 0.8; // Wider blend zone
        float blendEnd = u_vol.cascadeSplits[volCascade];
        if (dist > blendStart) {
            float blendT = (dist - blendStart) / (blendEnd - blendStart);
            float next_near = u_vol.cascadeSplits[volCascade];
            float next_far = u_vol.cascadeSplits[volCascade+1];
            float next_volZ = log(max(dist, next_near) / next_near) / log(next_far / next_near);

            vec4 next_volumetric;
            if (volCascade + 1 == 1) next_volumetric = texture(u_volumetricIntegrated[1], vec3(TexCoords, next_volZ));
            else if (volCascade + 1 == 2) next_volumetric = texture(u_volumetricIntegrated[2], vec3(TexCoords, next_volZ));
            else next_volumetric = texture(u_volumetricIntegrated[3], vec3(TexCoords, next_volZ));

            volumetric = mix(volumetric, next_volumetric, smoothstep(0.0, 1.0, blendT));
        }
    }

	vec3 result;
	if (!isSky) {
		// Terrain/objects: apply aerial perspective and clouds
		vec3 terrainAtmos = sceneColor * transmittance + inScattering;

		// Apply volumetric with balanced additive contribution
		// Attenuate scene color by transmittance, then add scattering
		terrainAtmos = terrainAtmos * volumetric.a + volumetric.rgb;

		vec3 cloudsAtmos = cloudColor * atmosTransmittance + atmosInScattering * (1.0 - cloudTransmittance);
		result = mix(cloudsAtmos, terrainAtmos, cloudTransmittance);
	} else {
		// Sky and colossal objects: preserve scene output (sun, moon, stars, colossal)
		// and blend clouds on top
		vec4 skyVol = texture(u_volumetricIntegrated[3], vec3(TexCoords, 1.0));
		// Sky scattering
		vec3 skyResult = sceneColor * skyVol.a + skyVol.rgb;

		vec3 cloudsAtmos = cloudColor * atmosTransmittance + atmosInScattering * (1.0 - cloudTransmittance);
		result = skyResult * cloudTransmittance + cloudsAtmos;
	}

	if (int(u_vol.timeParams.z) == 1) { // Density Debug
        // Transmittance is in .a, show it directly or invert for optical depth
        FragColor = vec4(vec3(1.0 - volumetric.a), 1.0);
    } else if (int(u_vol.timeParams.z) == 2) { // Scattering Debug
        FragColor = vec4(volumetric.rgb * 0.005, 1.0); // Consistent scale down for HDR visibility
    } else if (int(u_vol.timeParams.z) == 3) { // Cascade Debug
        vec3 cascadeColors[4] = { vec3(1,0,0), vec3(0,1,0), vec3(0,0,1), vec3(1,1,0) };
        FragColor = vec4(cascadeColors[volCascade], 1.0);
    } else if (int(u_vol.timeParams.z) == 4) { // Shadows Debug
        FragColor = vec4(volumetric.rgb * 0.005, 1.0);
    } else if (int(u_vol.timeParams.z) == 5) { // Voxel Shadow Debug
        FragColor = vec4(volumetric.rgb * 0.005, 1.0);
    } else {
        FragColor = vec4(result, 1.0);
    }
}
