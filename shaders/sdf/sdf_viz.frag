#version 430 core
out vec4 FragColor;

in vec3 FragPos;

uniform vec3 u_viewPos;
uniform sampler3D u_sdf_texture;
uniform vec3 u_min_bounds;
uniform vec3 u_max_bounds;
uniform mat4 u_invModel;
uniform float u_scale;

float sampleSDF(vec3 p_model) {
	vec3 size = u_max_bounds - u_min_bounds;
	if (length(size) < 0.001)
		return 1000.0;
	vec3 uvw = (p_model - u_min_bounds) / size;
	if (any(lessThan(uvw, vec3(0))) || any(greaterThan(uvw, vec3(1))))
		return 1000.0;
	return texture(u_sdf_texture, uvw).r;
}

void main() {
	vec3 rayDir = normalize(FragPos - u_viewPos);
	vec3 p = FragPos;

	float totalDist = 0.0;
	for (int i = 0; i < 256; i++) {
		vec3  p_model = vec3(u_invModel * vec4(p, 1.0));
		float d_model = sampleSDF(p_model);
		float d_world = d_model * u_scale;

		if (abs(d_world) < 0.005) {
			// Hit!
			vec3 size = u_max_bounds - u_min_bounds;
			vec3 uvw = (p_model - u_min_bounds) / size;
			vec3 normal = texture(u_sdf_texture, uvw).gba;

			// Transform normal to world space
			mat3 normalMatrix = transpose(mat3(u_invModel));
			vec3 worldNormal = normalize(normalMatrix * normal);

			vec3  lightDir = normalize(vec3(1.0, 1.0, 1.0));
			float diff = max(dot(worldNormal, lightDir), 0.2);
			FragColor = vec4(vec3(0.2, 0.6, 1.0) * diff, 1.0);
			return;
		}

		if (totalDist > 100.0)
			break;

		float step = max(abs(d_world), 0.01);
		p += rayDir * step;
		totalDist += step;
	}
	discard;
}
