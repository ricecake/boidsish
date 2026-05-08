#version 460 core

layout(location = 0) in vec3 aPos; // Not used, we use gl_VertexID

uniform vec3 uPulseOrigin;
uniform float uCurrentRadius;
uniform float uMaxRadius;
uniform int uMaxBounces;
uniform mat4 uView;
uniform mat4 uProj;
uniform vec3 uCameraPos;

uniform sampler2D uDepthTexture;
uniform sampler2D uNormalTexture;

#include "../types/temporal_data.glsl"

out float vIntensity;

vec3 fibonacciSphere(int i, int n) {
    float phi = acos(1.0 - 2.0 * (float(i) + 0.5) / float(n));
    float theta = 2.39996323 * float(i);
    return vec3(sin(phi) * cos(theta), sin(phi) * sin(theta), cos(phi));
}

vec3 worldToScreen(vec3 worldPos) {
    vec4 clip = uProj * uView * vec4(worldPos, 1.0);
    vec3 ndc = clip.xyz / clip.w;
    return ndc * 0.5 + 0.5;
}

void main() {
    int numRays = 100000;
    vec3 dir = fibonacciSphere(gl_VertexID, numRays);
    vec3 currentPos = uPulseOrigin;
    float remainingDist = uCurrentRadius;
    int bounces = 0;

    bool  isAtHitPoint = false;
    float stepSize = 1.0;

    // Raymarch segment by segment (each bounce is a segment)
    for (int b = 0; b <= uMaxBounces; ++b) {
        float segmentDist = 0.0;

        while (segmentDist < remainingDist) {
            float actualStep = min(stepSize, remainingDist - segmentDist);
            vec3  nextPos = currentPos + dir * actualStep;
            vec3  screenPos = worldToScreen(nextPos);

            if (screenPos.x >= 0.0 && screenPos.x <= 1.0 && screenPos.y >= 0.0 && screenPos.y <= 1.0) {
                float sceneDepth = texture(uDepthTexture, screenPos.xy).r;

                // If the next position is behind geometry
                if (screenPos.z > sceneDepth + 0.0001) {
                    // Hit!
                    // Reflect
                    vec3 viewNormal = texture(uNormalTexture, screenPos.xy).xyz;
                    vec3 worldNormal = normalize(mat3(invView) * viewNormal);
                    dir = reflect(dir, worldNormal);

                    remainingDist -= segmentDist;
                    // We don't advance currentPos here, we stay at the hit point for the next segment
                    // to avoid skipping geometry.
                    bounces++;
                    segmentDist = 0.0;

                    if (remainingDist < stepSize) {
                         // We are very close to the wavefront hit
                         isAtHitPoint = true;
                         break;
                    }

                    if (bounces > uMaxBounces) {
                        remainingDist = 0.0;
                        break;
                    }
                    continue;
                }
            }

            currentPos = nextPos;
            segmentDist += actualStep;
        }

        if (remainingDist <= 0.0 || isAtHitPoint) break;
    }

    // Only render if we ended up at a hit point
    if (!isAtHitPoint) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        return;
    }

    vec4 finalClip = uProj * uView * vec4(currentPos, 1.0);

    // Intensity fades as it expands
    float distFade = 1.0 - clamp(uCurrentRadius / uMaxRadius, 0.0, 1.0);
    vIntensity = distFade * 2.0; // Boost intensity

    gl_Position = finalClip;
    gl_PointSize = 4.0; // Larger points for better visibility

    // Discard if behind camera
    if (finalClip.w <= 0.0) gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
}
