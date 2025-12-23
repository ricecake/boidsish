#include "../helpers/noise.glsl"

//- Fragment shader logic for terrain
void fragment_terrain() {
	vec3  warp = vec3(fbm(vs_FragPos / 50 + time * 0.05));
	float nebula_noise = fbm(vs_FragPos / 50 + warp * 0.5);
	vec3  warpNoise = nebula_noise * warp;

	vec3 objectColor = mix(vec3(0.09, 0.09, 0.16), vec3(0.5, 0.8, 0.8), warpNoise);
	objectColor += warpNoise;

	float ambientStrength = 0.2;
	vec3  ambient = ambientStrength * lightColor;

	vec3  norm = normalize(vs_Normal);
	vec3  lightDir = normalize(lightPos - vs_FragPos);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3  diffuse = diff * lightColor;

	float specularStrength = 0.8;
	vec3  viewDir = normalize(viewPos - vs_FragPos);
	vec3  reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64);
	vec3  specular = specularStrength * spec * lightColor;

	vec3 result = vec3((ambient + diffuse) * objectColor + specular);

	float dist = length(vs_FragPos.xz - viewPos.xz);
	float fade_start = 540.0;
	float fade_end = 550.0;
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist);

	vec4 outColor = vec4(result, mix(0, fade, step(0.01, vs_FragPos.y)));
	outColor.a = abs(nebula_noise);
	FragColor = mix(
		vec4(0.0, 0.7, 0.7, mix(0, fade, step(0.01, vs_FragPos.y))) * length(outColor),
		outColor,
		step(1, fade)
	);
}
