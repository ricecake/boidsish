#version 430 core

in float    v_lifetime;
in vec4     view_pos;
flat in int v_style;
out vec4    FragColor;

// g = Anisotropy factor.
// Range: -1.0 to 1.0.
// 0 is isotropic (standard). Closer to 1.0 means more forward scattering (stronger "silver lining").
// For smoke, try values around 0.3 to 0.7.

// Temperature-based physics parameters (in Kelvin)
const float kAmbientTemp = 293.0;        // Room temperature (~20°C)
const float kExhaustInitTemp = 2000.0;   // Hot exhaust gas
const float kExplosionInitTemp = 3000.0; // Very hot explosion
const float kFireInitTemp = 1500.0;      // Fire temperature

// Cooling parameters (exponential decay: T = T0 * exp(-k*t))
const float kExhaustCoolingRate = 1.2;   // Fast cooling for exhaust
const float kExplosionCoolingRate = 0.8; // Moderate cooling for explosion
const float kFireCoolingRate = 0.6;      // Slow cooling for fire

// Physics parameters
const float kExhaustSpeed = 30.0;
const float kExhaustSpread = 0.1;
const float kExplosionSpeed = 30.0;
const float kFireSpeed = 1.0;
const float kFireSpread = 4.2;

// Temperature-dependent physics
const float kBuoyancyStrength = 0.3; // How much temperature affects upward force
const float kDragTempFactor = 0.1;   // How much temperature affects drag
const float kMinDrag = 0.5;          // Base drag coefficient
const float kDeathTemp = 550.0;      // Temperature below which particles die

float lifeToTemp(int style, float lifetime, float maxLifetime) {
	float maxTempt = 3000;
	if (v_style == 0) { // Rocket Trail
		maxTempt = 2000;
	} else if (v_style == 1) { // Explosion
		maxTempt = 3000;
	} else if (v_style == 2) { // Default Fire
		maxTempt = 1500;
	}

	return mix(kAmbientTemp, maxTempt, lifetime / maxLifetime);
}

float styleToLife(int style) {
	if (v_style == 0) { // Rocket Trail
		return 2.0;
	} else if (v_style == 1) { // Explosion
		return 2.5;
	} else if (v_style == 2) { // Default Fire
		return 5;
	}
}

float henyeyGreenstein(vec3 lightDir, vec3 viewDir, float g) {
	float cosTheta = dot(normalize(lightDir), normalize(viewDir));
	float g2 = g * g;

	// The main HG formula
	float numerator = 1.0 - g2;
	float denominator = 1.0 + g2 - 2.0 * g * cosTheta;

	// Pow 1.5 is standard, but you can tweak for performance/look
	return numerator / (4.0 * 3.14159 * pow(denominator, 1.5));
}

vec3 blackbody(float Temp) {
	// Temp expected in Kelvin, e.g., 1000.0 to 10000.0
	vec3 color = vec3(255.0, 255.0, 255.0);

	// Red
	if (Temp < 6600.0) {
		color.r = 255.0;
	} else {
		color.r = 329.698727446 * pow(Temp / 100.0 - 60.0, -0.1332047592);
	}

	// Green
	if (Temp < 6600.0) {
		color.g = 99.4708025861 * log(Temp / 100.0) - 161.1195681661;
	} else {
		color.g = 288.1221695283 * pow(Temp / 100.0 - 60.0, -0.0755148492);
	}

	// Blue
	if (Temp >= 6600.0) {
		color.b = 255.0;
	} else {
		if (Temp <= 1900.0) {
			color.b = 0.0;
		} else {
			color.b = 138.5177312231 * log(Temp / 100.0 - 10.0) - 305.0447927307;
		}
	}

	return clamp(color / 255.0, 0.0, 1.0);
}

// Enhanced blackbody with style-specific modifications
vec3 getParticleColor(float temp, int style) {
	vec3 blackbody_color = blackbody(temp);

	// Style-specific color modifications
	if (style == 0) { // Rocket Trail - more blue-shifted, metallic
		blackbody_color = mix(blackbody_color, blackbody_color * vec3(0.8, 0.9, 1.2), 0.3);
	} else if (style == 1) { // Explosion - more orange/yellow emphasis
		blackbody_color = mix(blackbody_color, blackbody_color * vec3(1.2, 1.1, 0.8), 0.2);
	} else if (style == 2) { // Fire - warmer, more red-orange
		blackbody_color = mix(blackbody_color, blackbody_color * vec3(1.1, 0.95, 0.7), 0.25);
	}

	return blackbody_color;
}

void main() {
	// Shape the point into a circle and discard fragments outside the circle
	vec2  circ = gl_PointCoord - vec2(0.5);
	float dist = dot(circ, circ);
	if (dist > 0.25) {
		discard;
	}

	vec3 color = getParticleColor(lifeToTemp(v_style, v_lifetime, styleToLife(v_style)), v_style);
	// if (v_style == 0) {                       // Rocket Trail
	// 	vec3 hot_color = vec3(0.8, 0.6, 0.6); // Bright blue-white
	// 	// vec3 mid_color = vec3(0.5, 0.5, 1.0);   // Blue
	// 	// vec3 cool_color = vec3(0.1, 0.1, 0.3);    // Dark blue
	// 	// vec3 smoke_color = vec3(0.3, 0.3, 0.3);
	// 	// vec3 mid_color = vec3(1.0, 0.5, 0.0);   // Orange
	// 	// vec3 cool_color = vec3(0.4, 0.1, 0.0);    // Dark red/smokey
	// 	// vec3 smoke_color = vec3(0.3, 0.3, 0.3);    // Dark red/smokey

	// 	vec3 mid_color = vec3(0.6, 0.3, 0.0);   // Orange
	// 	vec3 cool_color = vec3(0.2, 0.1, 0.0);  // Dark red/smokey
	// 	vec3 smoke_color = vec3(0.1, 0.0, 0.0); // Dark red/smokey

	// 	color = mix(
	// 		mix(smoke_color, cool_color, v_lifetime / 2.5),
	// 		mix(mid_color, hot_color, v_lifetime / 2.5),
	// 		v_lifetime / 5
	// 	);
	// 	// color = vec3(0, 0, 1);
	// } else if (v_style == 1) { // Explosion
	// 	vec3 hot_color = vec3(1.0, 1.0, 0.8);
	// 	// vec3 mid_color = vec3(1.0, 0.8, 0.2);
	// 	// vec3 cool_color = vec3(0.8, 0.2, 0.0);
	// 	// vec3 smoke_color = vec3(0.4, 0.4, 0.4);
	// 	vec3 mid_color = vec3(0.8, 0.6, 0.0);   // Orange
	// 	vec3 cool_color = vec3(0.2, 0.1, 0.0);  // Dark red/smokey
	// 	vec3 smoke_color = vec3(0.1, 0.0, 0.0); // Dark red/smokey

	// 	color = mix(
	// 		mix(smoke_color, cool_color, v_lifetime / 5),
	// 		mix(mid_color, hot_color, v_lifetime / 5),
	// 		v_lifetime / 10
	// 	);
	// } else if (v_style == 2) {                  // Default Fire
	// 	vec3 hot_color = vec3(0.7, 0.7, 0.4);   // Bright yellow-white
	// 	vec3 mid_color = vec3(0.7, 0.5, 0.0);   // Orange
	// 	vec3 cool_color = vec3(0.4, 0.1, 0.0);  // Dark red/smokey
	// 	vec3 smoke_color = vec3(0.2, 0.2, 0.0); // Dark red/smokey
	// 	color = mix(
	// 		mix(smoke_color, cool_color, v_lifetime / 2.5),
	// 		mix(mid_color, hot_color, v_lifetime / 2.5),
	// 		v_lifetime / 10.5
	// 	);
	// }

	// if (v_style == 28) {
	// 	// --- Iridescence Effect ---
	// 	// Fresnel term for the base reflectivity
	// 	float fresnel = pow(1.0 - dist, 5.0);

	// 	// Use view angle to create a color shift
	// 	float angle_factor = 1.0 - dist;
	// 	angle_factor = pow(angle_factor, 2.0);

	// 	// Use time and fragment position to create a swirling effect
	// 	float swirl = sin(v_lifetime * 0.5 + gl_PointCoord.y * 2.0) * 0.5 + 0.5;

	// 	// Combine for final color using a rainbow palette shifted by the swirl and angle
	// 	vec3 iridescent_color = vec3(
	// 		sin(angle_factor * 10.0 + swirl * 5.0) * 0.5 + 0.5,
	// 		sin(angle_factor * 10.0 + swirl * 5.0 + 2.0) * 0.5 + 0.5,
	// 		sin(angle_factor * 10.0 + swirl * 5.0 + 4.0) * 0.5 + 0.5
	// 	);

	// 	// Add a strong specular highlight
	// 	vec3  reflect_dir = reflect(-view_pos + vec4(0, 20, 0, 0), vec4(dist)).xyz;
	// 	float spec = pow(max(dot(view_pos.xyz, reflect_dir), 0.0), 128.0);
	// 	vec3  specular = 1.5 * spec * vec3(1.0); // white highlight

	// 	vec3 final_color = mix(iridescent_color, vec3(1.0), fresnel) + specular;

	// 	FragColor = vec4(final_color, 0.75); // Semi-transparent
	// } else if (v_style == 0) {
	// 	float alpha = 1 - length(color - vec3(0.1, 0.0, 0.0)); // whatever smoke color is
	// 	FragColor = vec4(color, alpha);
	// } else {
	// 	// float alpha = smoothstep((1.0 - v_lifetime), (v_lifetime), dist*v_lifetime / 2.5);
	// 	// float alpha = smoothstep(0.25, 0.75, dist * v_lifetime);
	// 	float alpha = smoothstep(0.25, 0.75, v_lifetime / 2.5);
	// 	FragColor = vec4(color, alpha);
	// }
	float alpha = mix(1, 0.1, v_lifetime / styleToLife(v_style));
	FragColor = vec4(color, alpha);
}