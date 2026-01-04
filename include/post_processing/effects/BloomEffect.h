#pragma once

#include <memory>
#include <vector>

#include "post_processing/IPostProcessingEffect.h"

// Forward declarations
class Shader;

namespace Boidsish {
namespace PostProcessing {

class BloomEffect : public IPostProcessingEffect {
public:
    BloomEffect(int width, int height);
    ~BloomEffect();

    void Initialize(int width, int height) override;
    void Apply(GLuint sourceTexture) override;
    void Resize(int width, int height) override;

    void SetIntensity(float intensity) { intensity_ = intensity; }
    float GetIntensity() const { return intensity_; }

    void SetThreshold(float threshold) { threshold_ = threshold; }
    float GetThreshold() const { return threshold_; }

private:
    void InitializeFBOs();

    std::unique_ptr<Shader> _brightPassShader;
    std::unique_ptr<Shader> _blurShader;
    std::unique_ptr<Shader> _compositeShader;
    std::unique_ptr<Shader> _passthroughShader;

    GLuint _outputFBO;
    GLuint _outputTexture;
    GLuint _brightPassFBO;
    GLuint _brightPassTexture;

    GLuint _pingpongFBO[2];
    GLuint _pingpongTexture[2];

    int _width, _height;
    float intensity_ = 0.8f;
    float threshold_ = 1.0f;
    unsigned int _blurAmount = 10;
};

} // namespace PostProcessing
} // namespace Boidsish
