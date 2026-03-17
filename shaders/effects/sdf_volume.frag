#version 430 core
#extension GL_GOOGLE_include_directive : enable

out vec4 FragColor;
in vec2  TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;
uniform vec2      screenSize;
uniform vec3      cameraPos;
uniform mat4      invView;
uniform mat4      invProjection;
uniform float     time;

struct SdfSource {
	vec4 position_radius;  // xyz: pos, w: radius
	vec4 color_smoothness; // rgb: color, a: smoothness
	vec4 params;           // x: charge, y: type, z: noise_intensity, w: noise_scale
};

layout(std140) uniform SdfVolumes {
	int       numSources;
	SdfSource sources[128];
};

#include "lygia/sdf/sphereSDF.glsl"
#include "../helpers/fast_noise.glsl"

// Custom union that handles color blending
vec4 opUnionColored(vec4 d1, vec4 d2, float k) {
	float h = clamp(0.5 + 0.5 * (d2.a - d1.a) / k, 0.0, 1.0);
	float res_d = mix(d2.a, d1.a, h) - k * h * (1.0 - h);
	vec3  res_col = mix(d2.rgb, d1.rgb, h);
	return vec4(res_col, res_d);
}

// Custom subtraction that handles color for "antimatter" effect
vec4 opSubtractionColored(vec4 d1, vec4 d2, float k) {
	float h = clamp(0.5 - 0.5 * (d2.a + d1.a) / k, 0.0, 1.0);
	float res_d = mix(d2.a, -d1.a, h) + k * h * (1.0 - h);
	// Blend predator color into the blob where it's being "eaten"
	vec3 res_col = mix(d2.rgb, d1.rgb, h);
	return vec4(res_col, res_d);
}

#define TYPE_SPHERE 0
#define TYPE_EXPLOSION 1

vec3 getFireColor(float heat) {
	heat = clamp(heat, 0.0, 1.0);
	vec3 red = vec3(0.8, 0.0, 0.0);
	vec3 orange = vec3(1.0, 0.4, 0.0);
	vec3 yellow = vec3(1.0, 0.8, 0.1);
	vec3 white = vec3(1.0, 1.0, 0.8);

	// Shifted heat thresholds to favor orange/red
	if (heat < 0.3)
		return mix(vec3(0.01), red, heat / 0.3);
	if (heat < 0.6)
		return mix(red, orange, (heat - 0.3) / 0.3);
	if (heat < 0.85)
		return mix(orange, yellow, (heat - 0.6) / 0.25);
	return 3*mix(yellow, white, (heat - 0.85) / 0.15);
}

vec4 map(vec3 p) {
	vec4 res = vec4(1.0, 1.0, 1.0, 1000.0);

	// First pass: Union of positive charges (Boids)
	bool first = true;
	for (int i = 0; i < numSources; ++i) {
		if (sources[i].params.x > 0.0) {
			float d;
			vec3  col = sources[i].color_smoothness.rgb;

			if (int(sources[i].params.y) == TYPE_EXPLOSION) {
				// Apply a second noise for alpha/density
				float alpha_noise = fastWorley3d(p*0.05 * sources[i].params.w + time*0.005);
				float noise = fastWarpedFbm3d(p * alpha_noise * sources[i].params.w + time * 0.02);
				noise = mix(noise, alpha_noise, fastSimplex3d(vec3(alpha_noise, noise, noise*alpha_noise)));
				d = sphereSDF(p - sources[i].position_radius.xyz, sources[i].position_radius.w);
				d += noise * sources[i].params.z;

				float heat = 1.0 - clamp(d / (sources[i].position_radius.w * 0.05), 0.0, 1.0);
				heat = pow(heat, 2.50); // Sharper falloff

				col = getFireColor(heat * sources[i].params.w + noise * 5.0)*2.0;
				// We pack albedo in rgb and density in a for later blending
				// But map() usually returns distance in .a, so we'll need to handle this in main.
			} else {
				d = sphereSDF(p - sources[i].position_radius.xyz, sources[i].position_radius.w);
			}

			if (first) {
				res = vec4(col, d);
				first = false;
			} else {
				res = opUnionColored(vec4(col, d), res, sources[i].color_smoothness.a);
			}
		}
	}

	// Second pass: Subtraction of negative charges (Predators)
	for (int i = 0; i < numSources; ++i) {
		if (sources[i].params.x < 0.0) {
			float d = sphereSDF(p - sources[i].position_radius.xyz, sources[i].position_radius.w);
			if (!first) {
				res = opSubtractionColored(
					vec4(sources[i].color_smoothness.rgb, d),
					res,
					sources[i].color_smoothness.a
				);
			}
		}
	}

	return res;
}

vec3 getNormal(vec3 p) {
	vec2 e = vec2(0.01, 0.0);
	return normalize(vec3(
		map(p + e.xyy).a - map(p - e.xyy).a,
		map(p + e.yxy).a - map(p - e.yxy).a,
		map(p + e.yyx).a - map(p - e.yyx).a
	));
}

void main() {
	vec3  sceneColor = texture(sceneTexture, TexCoords).rgb;
	float depth = texture(depthTexture, TexCoords).r;

	// Reconstruct scene world position to get depth limit
	vec4 ndcPos = vec4(TexCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 viewPos = invProjection * ndcPos;
	viewPos /= viewPos.w;
	vec4  worldPos = invView * viewPos;
	float sceneDistance = length(worldPos.xyz - cameraPos);
	if (depth >= 0.999999)
		sceneDistance = 10000.0;

	// Ray direction
	vec4 target = invProjection * vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
	vec3 rayDir = normalize((invView * vec4(normalize(target.xyz), 0.0)).xyz);

	float t = fastBlueNoise(target.xz);
	vec4  res;
	bool  hit = false;

	for (int i = 0; i < 96; ++i) { // Iteration limit
		vec3 p = cameraPos + rayDir * t;
		res = map(p);
		if (res.a < 0.01) {
			hit = true;
			break;
		}
		t += res.a;
		if (t > sceneDistance || t > 1500.0)
			break;
	}

	if (hit) {
		vec3  p = cameraPos + rayDir * t;
		vec3  normal = getNormal(p);
		vec3  lightDir = normalize(vec3(0.5, 1.0, 0.5));
		float diff = max(dot(normal, lightDir), 0.0);

		// Recalculate density at the hit point for explosion types
		float final_alpha = 1.0;
		// We need a way to know if we hit an explosion source
		// For simplicity, let's re-map at the hit point and check parameters
		// Actually, we can just use the color returned by map() which we tweaked

		// Add a bit of rim light/glow
		float rim = 1.0 - max(dot(normal, -rayDir), 0.0);
		rim = pow(rim, 3.0);

		vec3 volumeColor = res.rgb * (diff * 0.8 + 0.2) + res.rgb * rim * 0.5;

		// Approximate alpha based on distance to surface and noise
		// In a real volume renderer we'd accumulate, but here we're raymarching to surface
		// Let's use Worley noise again to create "holes" or transparent regions
		// float alpha_noise = fastWorley3d(p*0.05+fastCurl3d(p+time) - time * 0.5);
		// final_alpha = 1;//smoothstep(0.0, 0.2, 0.8 - alpha_noise);

		FragColor = vec4(mix(sceneColor, volumeColor, 1.0), 1.0);//smoothstep(0.5, 0.75, length(volumeColor))), 1.0);
	} else {
		FragColor = vec4(sceneColor, 1.0);
	}
}

// #define pi 3.14159265
#define R(p, a) p=cos(a)*p+sin(a)*vec2(p.y, -p.x)

// iq's noise
float noise( in vec3 x )
{
    vec3 p = floor(x);
    vec3 f = fract(x);
	f = f*f*(3.0-2.0*f);
	vec2 uv = (p.xy+vec2(37.0,17.0)*p.z) + f.xy;
	vec2 rg = textureLod( iChannel0, (uv+ 0.5)/256.0, 0.0 ).yx;
	return 1. - 0.82*mix( rg.x, rg.y, f.z );
}

float fbm( vec3 p )
{
   return noise(p*.06125)*.5 + noise(p*.125)*.25 + noise(p*.25)*.125 + noise(p*.4)*.2;
}

float Sphere( vec3 p, float r )
{
    return length(p)-r;
}

//==============================================================
// otaviogood's noise from https://www.shadertoy.com/view/ld2SzK
//--------------------------------------------------------------
// This spiral noise works by successively adding and rotating sin waves while increasing frequency.
// It should work the same on all computers since it's not based on a hash function like some other noises.
// It can be much faster than other noise functions if you're ok with some repetition.
const float nudge = 4.;	// size of perpendicular vector
float normalizer = 1.0 / sqrt(1.0 + nudge*nudge);	// pythagorean theorem on that perpendicular to maintain scale
float SpiralNoiseC(vec3 p)
{
    float n = -mod(iTime * 1.2,-2.); // noise amount
    float iter = 2.0;
    for (int i = 0; i < 8; i++)
    {
        // add sin and cos scaled inverse with the frequency
        n += -abs(sin(p.y*iter) + cos(p.x*iter)) / iter;	// abs for a ridged look
        // rotate by adding perpendicular and scaling down
        p.xy += vec2(p.y, -p.x) * nudge;
        p.xy *= normalizer;
        // rotate on other axis
        p.xz += vec2(p.z, -p.x) * nudge;
        p.xz *= normalizer;
        // increase the frequency
        iter *= 1.733733;
    }
    return n;
}

float VolumetricExplosion(vec3 p)
{
    float final = Sphere(p,4.);
    final += fbm(p*50.);
    final += SpiralNoiseC(p.zxy*0.4132+333.)*3.0; //1.25;

    return final;
}

float map(vec3 p)
{
	R(p.xz, iMouse.x*0.008*pi+iTime*0.1);

	float VolExplosion = VolumetricExplosion(p/0.5)*0.5; // scale

	return VolExplosion;
}
//--------------------------------------------------------------

// assign color to the media
vec3 computeColor( float density, float radius )
{
	// color based on density alone, gives impression of occlusion within
	// the media
	vec3 result = mix( vec3(1.0,0.9,0.8), vec3(0.4,0.15,0.1), density );

	// color added to the media
	vec3 colCenter = 7.*vec3(0.8,1.0,1.0);
	vec3 colEdge = 1.5*vec3(0.48,0.53,0.5);
	result *= mix( colCenter, colEdge, min( (radius+.05)/.9, 1.15 ) );

	return result;
}

bool RaySphereIntersect(vec3 org, vec3 dir, out float near, out float far)
{
	float b = dot(dir, org);
	float c = dot(org, org) - 8.;
	float delta = b*b - c;
	if( delta < 0.0)
		return false;
	float deltasqrt = sqrt(delta);
	near = -b - deltasqrt;
	far = -b + deltasqrt;
	return far > 0.0;
}

// Applies the filmic curve from John Hable's presentation
// More details at : http://filmicgames.com/archives/75
vec3 ToneMapFilmicALU(vec3 _color)
{
	_color = max(vec3(0), _color - vec3(0.004));
	_color = (_color * (6.2*_color + vec3(0.5))) / (_color * (6.2 * _color + vec3(1.7)) + vec3(0.06));
	return _color;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    const float KEY_1 = 49.5/256.0;
	const float KEY_2 = 50.5/256.0;
	const float KEY_3 = 51.5/256.0;
    float key = 0.0;
    key += 0.7*texture(iChannel1, vec2(KEY_1,0.25)).x;
    key += 0.7*texture(iChannel1, vec2(KEY_2,0.25)).x;
    key += 0.7*texture(iChannel1, vec2(KEY_3,0.25)).x;

    vec2 uv = fragCoord/iResolution.xy;

	// ro: ray origin
	// rd: direction of the ray
	vec3 rd = normalize(vec3((fragCoord.xy-0.5*iResolution.xy)/iResolution.y, 1.));
	vec3 ro = vec3(0., 0., -6.+key*1.6);

	// ld, td: local, total density
	// w: weighting factor
	float ld=0., td=0., w=0.;

	// t: length of the ray
	// d: distance function
	float d=1., t=0.;

    const float h = 0.1;

	vec4 sum = vec4(0.0);

    float min_dist=0.0, max_dist=0.0;

    if(RaySphereIntersect(ro, rd, min_dist, max_dist))
    {

	t = min_dist*step(t,min_dist);

	// raymarch loop
    for (int i=0; i<86; i++)
	{

		vec3 pos = ro + t*rd;

		// Loop break conditions.
	    if(td>0.9 || d<0.12*t || t>10. || sum.a > 0.99 || t>max_dist) break;

        // evaluate distance function
        float d = map(pos);

        d = abs(d)+0.07;

		// change this string to control density
		d = max(d,0.03);

        // point light calculations
        vec3 ldst = vec3(0.0)-pos;
        float lDist = max(length(ldst), 0.001);

        // the color of light
        vec3 lightColor=vec3(1.0,0.5,0.25);

        sum.rgb+=(lightColor/exp(lDist*lDist*lDist*.08)/30.); // bloom

		if (d<h)
		{
			// compute local density
			ld = h - d;

            // compute weighting factor
			w = (1. - td) * ld;

			// accumulate density
			td += w + 1./200.;

			vec4 col = vec4( computeColor(td,lDist), td );

            // emission
            sum += sum.a * vec4(sum.rgb, 0.0) * 0.2 / lDist;

			// uniform scale density
			col.a *= 0.2;
			// colour by alpha
			col.rgb *= col.a;
			// alpha blend in contribution
			sum = sum + col*(1.0 - sum.a);

		}

		td += 1./70.;

        vec2 uvd = uv;
        uvd.y*=120.;
        uvd.x*=280.;
        d=abs(d)*(.8+0.08*texture(iChannel2,vec2(uvd.y,-uvd.x+0.5*sin(4.0*iTime+uvd.y*4.0))).r); // replace with fastBlueNoise

        // trying to optimize step size
        t += max(d * 0.08 * max(min(length(ldst),d),2.0), 0.01);


	}

    // simple scattering
    sum *= 1. / exp( ld * 0.2 ) * 0.8;

   	sum = clamp( sum, 0.0, 1.0 );

    sum.xyz = sum.xyz*sum.xyz*(3.0-2.0*sum.xyz);

	}

    fragColor = vec4(sum.xyz,1.0);
