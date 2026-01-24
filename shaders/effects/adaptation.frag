#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D lumTexture;
uniform sampler2D lastFrameLumTexture;
uniform float     deltaTime;
uniform float     adaptationSpeedUp;
uniform float     adaptationSpeedDown;
uniform float     targetLuminance;
uniform vec2      exposureClamp;

void main() {
    float scene_luminance = texture(lumTexture, vec2(0.5)).r;
    float last_frame_luminance = texture(lastFrameLumTexture, vec2(0.5)).r;
    float last_frame_exposure = texture(lastFrameLumTexture, vec2(0.5)).g;
    float target_exposure = targetLuminance / (scene_luminance + 0.0001);

    float adaptation_speed = scene_luminance > last_frame_luminance ? adaptationSpeedUp : adaptationSpeedDown;
    float current_exposure = last_frame_exposure + (target_exposure - last_frame_exposure) * adaptation_speed * deltaTime;
    current_exposure = clamp(current_exposure, exposureClamp.x, exposureClamp.y);

    FragColor = vec4(scene_luminance, current_exposure, 0.0, 1.0);
}
