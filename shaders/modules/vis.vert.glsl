//- Vertex shader logic for standard visualization
void vertex_vis() {
	vec3 displacedPos = aPos;
	vec3 displacedNormal = aNormal;

	displacedPos = applyGlitch(displacedPos, time);

	if (ripple_strength > 0.0) {
		float frequency = 20.0;
		float speed = 3.0;
		float amplitude = ripple_strength;

		float wave = sin(frequency * (aPos.x + aPos.z) + time * speed);
		displacedPos = aPos + aNormal * wave * amplitude;

		vec3 gradient = vec3(
			cos(frequency * (aPos.x + aPos.z) + time * speed) * frequency * amplitude,
			0.0,
			cos(frequency * (aPos.x + aPos.z) + time * speed) * frequency * amplitude
		);
		displacedNormal = normalize(aNormal - gradient);
	}

	vs_FragPos = vec3(model * vec4(displacedPos, 1.0));
	vs_Normal = mat3(transpose(inverse(model))) * displacedNormal;
	vs_color = aColor;
	vs_barycentric = getBarycentric();
	gl_Position = projection * view * vec4(vs_FragPos, 1.0);
	gl_ClipDistance[0] = dot(vec4(vs_FragPos, 1.0), clipPlane);
}
