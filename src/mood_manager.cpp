#include "mood_manager.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

    // Helper for Oklab interpolation
    struct Oklab {
        float L, a, b;
    };

    static Oklab LinearToOklab(glm::vec3 c) {
        // Avoid zeros for log/pow
        c = glm::max(c, glm::vec3(1e-6f));

        float l = 0.4122214708f * c.r + 0.5363325363f * c.g + 0.0514459929f * c.b;
        float m = 0.2119034982f * c.r + 0.6806995451f * c.g + 0.1073969566f * c.b;
        float s = 0.0883024619f * c.r + 0.2817188376f * c.g + 0.6299787005f * c.b;

        float l_ = std::pow(l, 1.0f/3.0f);
        float m_ = std::pow(m, 1.0f/3.0f);
        float s_ = std::pow(s, 1.0f/3.0f);

        return {
            0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_,
            1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_,
            0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_
        };
    }

    static glm::vec3 OklabToLinear(Oklab c) {
        float l_ = c.L + 0.3963377774f * c.a + 0.2158037573f * c.b;
        float m_ = c.L - 0.1055613458f * c.a - 0.0638541728f * c.b;
        float s_ = c.L - 0.0894841775f * c.a - 1.2914855480f * c.b;

        float l = l_ * l_ * l_;
        float m = m_ * m_ * m_;
        float s = s_ * s_ * s_;

        return {
            4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
            -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
            -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s
        };
    }

    // Generic Catmull-Rom for float
    static float CatmullRom(float t, float p0, float p1, float p2, float p3) {
        return 0.5f * (
            (2.0f * p1) +
            (-p0 + p2) * t +
            (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * (t * t) +
            (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * (t * t * t)
        );
    }

    // Generic Catmull-Rom for glm types
    template<typename T>
    static T CatmullRomT(float t, T p0, T p1, T p2, T p3) {
        return 0.5f * (
            (2.0f * p1) +
            (-p0 + p2) * t +
            (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * (t * t) +
            (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * (t * t * t)
        );
    }

    MoodManager::MoodManager() {
        _blendedSettings = {};
    }

    MoodManager::~MoodManager() {}

    void MoodManager::Update(const std::map<MoodParameter, float>& currentParams) {
        _currentParams = currentParams;

        std::vector<MoodLayer> sortedLayers = _layers;
        std::sort(sortedLayers.begin(), sortedLayers.end(), [](const MoodLayer& a, const MoodLayer& b) {
            return a.priority < b.priority;
        });

        bool first = true;
        for (const auto& layer : sortedLayers) {
            if (!layer.enabled || layer.controlPoints.empty()) continue;

            float paramValue = 0.0f;
            if (_currentParams.count(layer.trackedParameter)) {
                paramValue = _currentParams.at(layer.trackedParameter);
            }

            MoodSettings layerContribution = Interpolate(layer, paramValue);

            if (first) {
                _blendedSettings = layerContribution;
                first = false;
            } else {
                Blend(_blendedSettings, layerContribution, layer.blendMode);
            }
        }

        if (_overrideEnabled) {
            _blendedSettings = _userOverride;
        }
    }

    void MoodManager::AddLayer(const MoodLayer& layer) {
        _layers.push_back(layer);
    }

    void MoodManager::RemoveLayer(const std::string& name) {
        _layers.erase(std::remove_if(_layers.begin(), _layers.end(), [&](const MoodLayer& l) {
            return l.name == name;
        }), _layers.end());
    }

    void MoodManager::SetLayerEnabled(const std::string& name, bool enabled) {
        for (auto& layer : _layers) {
            if (layer.name == name) {
                layer.enabled = enabled;
            }
        }
    }

    void MoodManager::SetOverride(const MoodSettings& settings, bool enabled) {
        _userOverride = settings;
        _overrideEnabled = enabled;
    }

    enum class InterpType {
        Linear,
        Logarithmic,
        Oklab,
        Slerp,
        Boolean,
        Integer
    };

    template<typename T>
    T InterpVal(float t, T v0, T v1, T v2, T v3, InterpType type) {
        switch(type) {
            case InterpType::Logarithmic: {
                // Handle float or glm types (component-wise)
                auto l0 = glm::log(glm::max(v0, T(1e-6f)));
                auto l1 = glm::log(glm::max(v1, T(1e-6f)));
                auto l2 = glm::log(glm::max(v2, T(1e-6f)));
                auto l3 = glm::log(glm::max(v3, T(1e-6f)));
                return glm::exp(CatmullRomT(t, l0, l1, l2, l3));
            }
            case InterpType::Oklab: {
                // Specialized for vec3
                if constexpr (std::is_same_v<T, glm::vec3>) {
                    auto o0 = LinearToOklab(v0);
                    auto o1 = LinearToOklab(v1);
                    auto o2 = LinearToOklab(v2);
                    auto o3 = LinearToOklab(v3);
                    Oklab res;
                    res.L = CatmullRom(t, o0.L, o1.L, o2.L, o3.L);
                    res.a = CatmullRom(t, o0.a, o1.a, o2.a, o3.a);
                    res.b = CatmullRom(t, o0.b, o1.b, o2.b, o3.b);
                    return OklabToLinear(res);
                }
                return CatmullRomT(t, v0, v1, v2, v3);
            }
            case InterpType::Slerp: {
                if constexpr (std::is_same_v<T, glm::vec3>) {
                    // This is a bit tricky for Catmull-Rom with Slerp
                    // Simplification: just use linear for Catmull-Rom if not pure Lerp
                    // or implement a proper spherical spline. For now, lerp.
                    return CatmullRomT(t, v0, v1, v2, v3);
                }
                return CatmullRomT(t, v0, v1, v2, v3);
            }
            case InterpType::Boolean:
            case InterpType::Integer:
                return (t < 0.5f) ? v1 : v2;
            case InterpType::Linear:
            default:
                return CatmullRomT(t, v0, v1, v2, v3);
        }
    }

    static void InterpBloom(float t, const PostProcessing::BloomEffect::LayerSettings& s0,
                           const PostProcessing::BloomEffect::LayerSettings& s1,
                           const PostProcessing::BloomEffect::LayerSettings& s2,
                           const PostProcessing::BloomEffect::LayerSettings& s3,
                           PostProcessing::BloomEffect::LayerSettings& res) {
        res.toneMappingEnabled = InterpVal(t, s0.toneMappingEnabled, s1.toneMappingEnabled, s2.toneMappingEnabled, s3.toneMappingEnabled, InterpType::Boolean);
        res.toneMappingMode = InterpVal(t, s0.toneMappingMode, s1.toneMappingMode, s2.toneMappingMode, s3.toneMappingMode, InterpType::Integer);
        res.autoExposureEnabled = InterpVal(t, s0.autoExposureEnabled, s1.autoExposureEnabled, s2.autoExposureEnabled, s3.autoExposureEnabled, InterpType::Boolean);

        res.targetLuminance = InterpVal(t, s0.targetLuminance, s1.targetLuminance, s2.targetLuminance, s3.targetLuminance, InterpType::Logarithmic);
        res.minExposure = InterpVal(t, s0.minExposure, s1.minExposure, s2.minExposure, s3.minExposure, InterpType::Logarithmic);
        res.maxExposure = InterpVal(t, s0.maxExposure, s1.maxExposure, s2.maxExposure, s3.maxExposure, InterpType::Logarithmic);

        res.speedUp = InterpVal(t, s0.speedUp, s1.speedUp, s2.speedUp, s3.speedUp, InterpType::Linear);
        res.speedDown = InterpVal(t, s0.speedDown, s1.speedDown, s2.speedDown, s3.speedDown, InterpType::Linear);
        res.centerWeightTightness = InterpVal(t, s0.centerWeightTightness, s1.centerWeightTightness, s2.centerWeightTightness, s3.centerWeightTightness, InterpType::Linear);
        res.focusPoint = InterpVal(t, s0.focusPoint, s1.focusPoint, s2.focusPoint, s3.focusPoint, InterpType::Linear);
        res.histogramLowCutoff = InterpVal(t, s0.histogramLowCutoff, s1.histogramLowCutoff, s2.histogramLowCutoff, s3.histogramLowCutoff, InterpType::Linear);
        res.histogramHighCutoff = InterpVal(t, s0.histogramHighCutoff, s1.histogramHighCutoff, s2.histogramHighCutoff, s3.histogramHighCutoff, InterpType::Linear);

        res.uchimuraP = InterpVal(t, s0.uchimuraP, s1.uchimuraP, s2.uchimuraP, s3.uchimuraP, InterpType::Linear);
        res.uchimuraA = InterpVal(t, s0.uchimuraA, s1.uchimuraA, s2.uchimuraA, s3.uchimuraA, InterpType::Linear);
        res.uchimuraM = InterpVal(t, s0.uchimuraM, s1.uchimuraM, s2.uchimuraM, s3.uchimuraM, InterpType::Linear);
        res.uchimuraL = InterpVal(t, s0.uchimuraL, s1.uchimuraL, s2.uchimuraL, s3.uchimuraL, InterpType::Linear);
        res.uchimuraC = InterpVal(t, s0.uchimuraC, s1.uchimuraC, s2.uchimuraC, s3.uchimuraC, InterpType::Linear);
        res.uchimuraB = InterpVal(t, s0.uchimuraB, s1.uchimuraB, s2.uchimuraB, s3.uchimuraB, InterpType::Linear);

        res.autoTuneEnabled = InterpVal(t, s0.autoTuneEnabled, s1.autoTuneEnabled, s2.autoTuneEnabled, s3.autoTuneEnabled, InterpType::Boolean);
        res.minContrast = InterpVal(t, s0.minContrast, s1.minContrast, s2.minContrast, s3.minContrast, InterpType::Linear);
        res.maxContrast = InterpVal(t, s0.maxContrast, s1.maxContrast, s2.maxContrast, s3.maxContrast, InterpType::Linear);
        res.targetBrightness = InterpVal(t, s0.targetBrightness, s1.targetBrightness, s2.targetBrightness, s3.targetBrightness, InterpType::Linear);

        res.cdlSlope = InterpVal(t, s0.cdlSlope, s1.cdlSlope, s2.cdlSlope, s3.cdlSlope, InterpType::Linear);
        res.cdlOffset = InterpVal(t, s0.cdlOffset, s1.cdlOffset, s2.cdlOffset, s3.cdlOffset, InterpType::Linear);
        res.cdlPower = InterpVal(t, s0.cdlPower, s1.cdlPower, s2.cdlPower, s3.cdlPower, InterpType::Logarithmic);
        res.cdlSaturation = InterpVal(t, s0.cdlSaturation, s1.cdlSaturation, s2.cdlSaturation, s3.cdlSaturation, InterpType::Linear);

        res.whiteTemp = InterpVal(t, s0.whiteTemp, s1.whiteTemp, s2.whiteTemp, s3.whiteTemp, InterpType::Linear);
        res.whiteTint = InterpVal(t, s0.whiteTint, s1.whiteTint, s2.whiteTint, s3.whiteTint, InterpType::Linear);

        res.ltmEnabled = InterpVal(t, s0.ltmEnabled, s1.ltmEnabled, s2.ltmEnabled, s3.ltmEnabled, InterpType::Boolean);
        res.ltmEvSpread = InterpVal(t, s0.ltmEvSpread, s1.ltmEvSpread, s2.ltmEvSpread, s3.ltmEvSpread, InterpType::Linear);
        res.ltmTarget = InterpVal(t, s0.ltmTarget, s1.ltmTarget, s2.ltmTarget, s3.ltmTarget, InterpType::Linear);
        res.ltmSigma = InterpVal(t, s0.ltmSigma, s1.ltmSigma, s2.ltmSigma, s3.ltmSigma, InterpType::Linear);
        res.ltmWeightContrast = InterpVal(t, s0.ltmWeightContrast, s1.ltmWeightContrast, s2.ltmWeightContrast, s3.ltmWeightContrast, InterpType::Linear);
        res.ltmWeightSaturation = InterpVal(t, s0.ltmWeightSaturation, s1.ltmWeightSaturation, s2.ltmWeightSaturation, s3.ltmWeightSaturation, InterpType::Linear);
        res.ltmWeightExposedness = InterpVal(t, s0.ltmWeightExposedness, s1.ltmWeightExposedness, s2.ltmWeightExposedness, s3.ltmWeightExposedness, InterpType::Linear);
        res.ltmBoostLocalContrast = InterpVal(t, s0.ltmBoostLocalContrast, s1.ltmBoostLocalContrast, s2.ltmBoostLocalContrast, s3.ltmBoostLocalContrast, InterpType::Linear);
    }

    MoodSettings MoodManager::Interpolate(const MoodLayer& layer, float paramValue) {
        const auto& pts = layer.controlPoints;
        if (pts.empty()) return MoodSettings{};
        if (pts.size() == 1) return pts[0].settings;

        // Find segments
        int i = 0;
        while (i < (int)pts.size() - 1 && pts[i+1].parameterValue < paramValue) {
            i++;
        }

        // Segment between i and i+1
        int i0 = std::max(0, i - 1);
        int i1 = i;
        int i2 = std::min((int)pts.size() - 1, i + 1);
        int i3 = std::min((int)pts.size() - 1, i + 2);

        float t = (paramValue - pts[i1].parameterValue) / (pts[i2].parameterValue - pts[i1].parameterValue);
        t = glm::clamp(t, 0.0f, 1.0f);

        const MoodSettings& s0 = pts[i0].settings;
        const MoodSettings& s1 = pts[i1].settings;
        const MoodSettings& s2 = pts[i2].settings;
        const MoodSettings& s3 = pts[i3].settings;

        MoodSettings res;
        InterpBloom(t, s0.sceneBloom, s1.sceneBloom, s2.sceneBloom, s3.sceneBloom, res.sceneBloom);
        InterpBloom(t, s0.skyBloom, s1.skyBloom, s2.skyBloom, s3.skyBloom, res.skyBloom);

        res.cloudDensity = InterpVal(t, s0.cloudDensity, s1.cloudDensity, s2.cloudDensity, s3.cloudDensity, InterpType::Logarithmic);
        res.cloudAltitude = InterpVal(t, s0.cloudAltitude, s1.cloudAltitude, s2.cloudAltitude, s3.cloudAltitude, InterpType::Linear);
        res.cloudThickness = InterpVal(t, s0.cloudThickness, s1.cloudThickness, s2.cloudThickness, s3.cloudThickness, InterpType::Linear);
        res.cloudColor = InterpVal(t, s0.cloudColor, s1.cloudColor, s2.cloudColor, s3.cloudColor, InterpType::Oklab);
        res.cloudCoverage = InterpVal(t, s0.cloudCoverage, s1.cloudCoverage, s2.cloudCoverage, s3.cloudCoverage, InterpType::Linear);
        res.cloudSunLightScale = InterpVal(t, s0.cloudSunLightScale, s1.cloudSunLightScale, s2.cloudSunLightScale, s3.cloudSunLightScale, InterpType::Logarithmic);
        res.cloudMoonLightScale = InterpVal(t, s0.cloudMoonLightScale, s1.cloudMoonLightScale, s2.cloudMoonLightScale, s3.cloudMoonLightScale, InterpType::Logarithmic);
        res.cloudPowderScale = InterpVal(t, s0.cloudPowderScale, s1.cloudPowderScale, s2.cloudPowderScale, s3.cloudPowderScale, InterpType::Logarithmic);
        res.cloudBeerPowderMix = InterpVal(t, s0.cloudBeerPowderMix, s1.cloudBeerPowderMix, s2.cloudBeerPowderMix, s3.cloudBeerPowderMix, InterpType::Linear);

        res.rayleighScale = InterpVal(t, s0.rayleighScale, s1.rayleighScale, s2.rayleighScale, s3.rayleighScale, InterpType::Logarithmic);
        res.mieScale = InterpVal(t, s0.mieScale, s1.mieScale, s2.mieScale, s3.mieScale, InterpType::Logarithmic);
        res.rayleighScattering = InterpVal(t, s0.rayleighScattering, s1.rayleighScattering, s2.rayleighScattering, s3.rayleighScattering, InterpType::Oklab);
        res.mieScattering = InterpVal(t, s0.mieScattering, s1.mieScattering, s2.mieScattering, s3.mieScattering, InterpType::Logarithmic);
        res.mieExtinction = InterpVal(t, s0.mieExtinction, s1.mieExtinction, s2.mieExtinction, s3.mieExtinction, InterpType::Logarithmic);

        return res;
    }

    template<typename T>
    void BlendVal(T& base, const T& layer, MoodBlendMode mode, InterpType type = InterpType::Linear) {
        switch(mode) {
            case MoodBlendMode::Add: base += layer; break;
            case MoodBlendMode::Subtract: base -= layer; break;
            case MoodBlendMode::Multiply: base *= layer; break;
            case MoodBlendMode::Divide: base /= glm::max(layer, T(1e-6f)); break;
            case MoodBlendMode::Override: base = layer; break;
        }
    }

    static void BlendBloom(PostProcessing::BloomEffect::LayerSettings& base, const PostProcessing::BloomEffect::LayerSettings& layer, MoodBlendMode mode) {
        if (mode == MoodBlendMode::Override) {
            base = layer;
            return;
        }
        // For Add/Sub/Mult/Div we only blend continuous parameters
        BlendVal(base.targetLuminance, layer.targetLuminance, mode);
        BlendVal(base.minExposure, layer.minExposure, mode);
        BlendVal(base.maxExposure, layer.maxExposure, mode);
        BlendVal(base.speedUp, layer.speedUp, mode);
        BlendVal(base.speedDown, layer.speedDown, mode);
        BlendVal(base.centerWeightTightness, layer.centerWeightTightness, mode);
        BlendVal(base.focusPoint, layer.focusPoint, mode);
        BlendVal(base.histogramLowCutoff, layer.histogramLowCutoff, mode);
        BlendVal(base.histogramHighCutoff, layer.histogramHighCutoff, mode);
        BlendVal(base.uchimuraP, layer.uchimuraP, mode);
        BlendVal(base.uchimuraA, layer.uchimuraA, mode);
        BlendVal(base.uchimuraM, layer.uchimuraM, mode);
        BlendVal(base.uchimuraL, layer.uchimuraL, mode);
        BlendVal(base.uchimuraC, layer.uchimuraC, mode);
        BlendVal(base.uchimuraB, layer.uchimuraB, mode);
        BlendVal(base.minContrast, layer.minContrast, mode);
        BlendVal(base.maxContrast, layer.maxContrast, mode);
        BlendVal(base.targetBrightness, layer.targetBrightness, mode);
        BlendVal(base.cdlSlope, layer.cdlSlope, mode);
        BlendVal(base.cdlOffset, layer.cdlOffset, mode);
        BlendVal(base.cdlPower, layer.cdlPower, mode);
        BlendVal(base.cdlSaturation, layer.cdlSaturation, mode);
        BlendVal(base.whiteTemp, layer.whiteTemp, mode);
        BlendVal(base.whiteTint, layer.whiteTint, mode);
        BlendVal(base.ltmEvSpread, layer.ltmEvSpread, mode);
        BlendVal(base.ltmTarget, layer.ltmTarget, mode);
        BlendVal(base.ltmSigma, layer.ltmSigma, mode);
        BlendVal(base.ltmWeightContrast, layer.ltmWeightContrast, mode);
        BlendVal(base.ltmWeightSaturation, layer.ltmWeightSaturation, mode);
        BlendVal(base.ltmWeightExposedness, layer.ltmWeightExposedness, mode);
        BlendVal(base.ltmBoostLocalContrast, layer.ltmBoostLocalContrast, mode);
    }

    void MoodManager::Blend(MoodSettings& base, const MoodSettings& layer, MoodBlendMode mode) {
        BlendBloom(base.sceneBloom, layer.sceneBloom, mode);
        BlendBloom(base.skyBloom, layer.skyBloom, mode);

        BlendVal(base.cloudDensity, layer.cloudDensity, mode);
        BlendVal(base.cloudAltitude, layer.cloudAltitude, mode);
        BlendVal(base.cloudThickness, layer.cloudThickness, mode);
        BlendVal(base.cloudColor, layer.cloudColor, mode);
        BlendVal(base.cloudCoverage, layer.cloudCoverage, mode);
        BlendVal(base.cloudSunLightScale, layer.cloudSunLightScale, mode);
        BlendVal(base.cloudMoonLightScale, layer.cloudMoonLightScale, mode);
        BlendVal(base.cloudPowderScale, layer.cloudPowderScale, mode);
        BlendVal(base.cloudBeerPowderMix, layer.cloudBeerPowderMix, mode);

        BlendVal(base.rayleighScale, layer.rayleighScale, mode);
        BlendVal(base.mieScale, layer.mieScale, mode);
        BlendVal(base.rayleighScattering, layer.rayleighScattering, mode);
        BlendVal(base.mieScattering, layer.mieScattering, mode);
        BlendVal(base.mieExtinction, layer.mieExtinction, mode);
    }

} // namespace Boidsish
