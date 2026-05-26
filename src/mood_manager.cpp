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

    template<typename T>
    static T CatmullRomT(float t, T p0, T p1, T p2, T p3) {
        return 0.5f * (
            (2.0f * p1) +
            (-p0 + p2) * t +
            (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * (t * t) +
            (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * (t * t * t)
        );
    }

    MoodManager::MoodManager() {}
    MoodManager::~MoodManager() {}

    void MoodManager::Update(const std::map<MoodParameter, float>& currentParams, float deltaTime) {
        _currentParams = currentParams;
        _blendedSettings = {};

        std::vector<MoodLayer> sortedLayers = _layers;
        std::sort(sortedLayers.begin(), sortedLayers.end(), [](const MoodLayer& a, const MoodLayer& b) {
            return a.priority < b.priority;
        });

        for (const auto& layer : sortedLayers) {
            if (!layer.enabled || layer.controlPoints.empty()) continue;

            float paramValue = 0.0f;
            if (_currentParams.count(layer.trackedParameter)) {
                paramValue = _currentParams.at(layer.trackedParameter);
            }

            MoodSettings layerContribution = Interpolate(layer, paramValue);
            Blend(_blendedSettings, layerContribution, layer.blendMode);
        }

        if (_overrideEnabled) {
            _blendedSettings = _userOverride;
        }

        Smooth(_smoothedSettings, _blendedSettings, deltaTime);
    }

    void MoodManager::AddLayer(const MoodLayer& layer) { _layers.push_back(layer); }
    void MoodManager::RemoveLayer(const std::string& name) {
        _layers.erase(std::remove_if(_layers.begin(), _layers.end(), [&](const MoodLayer& l) { return l.name == name; }), _layers.end());
    }
    void MoodManager::SetLayerEnabled(const std::string& name, bool enabled) {
        for (auto& layer : _layers) if (layer.name == name) layer.enabled = enabled;
    }
    void MoodManager::SetOverride(const MoodSettings& settings, bool enabled) {
        _userOverride = settings;
        _overrideEnabled = enabled;
    }

    enum class InterpType { Linear, Logarithmic, Oklab, Slerp, Boolean, Integer };

    template<typename T>
    T InterpVal(float t, T v0, T v1, T v2, T v3, InterpType type) {
        switch(type) {
            case InterpType::Logarithmic: {
                auto l0 = glm::log(glm::max(v0, T(1e-6f)));
                auto l1 = glm::log(glm::max(v1, T(1e-6f)));
                auto l2 = glm::log(glm::max(v2, T(1e-6f)));
                auto l3 = glm::log(glm::max(v3, T(1e-6f)));
                return glm::exp(CatmullRomT(t, l0, l1, l2, l3));
            }
            case InterpType::Oklab: {
                if constexpr (std::is_same_v<T, glm::vec3>) {
                    auto o0 = LinearToOklab(v0);
                    auto o1 = LinearToOklab(v1);
                    auto o2 = LinearToOklab(v2);
                    auto o3 = LinearToOklab(v3);
                    return OklabToLinear({
                        CatmullRomT(t, o0.L, o1.L, o2.L, o3.L),
                        CatmullRomT(t, o0.a, o1.a, o2.a, o3.a),
                        CatmullRomT(t, o0.b, o1.b, o2.b, o3.b)
                    });
                }
                return CatmullRomT(t, v0, v1, v2, v3);
            }
            case InterpType::Boolean:
            case InterpType::Integer: return (t < 0.5f) ? v1 : v2;
            case InterpType::Linear:
            default: return CatmullRomT(t, v0, v1, v2, v3);
        }
    }

    template<typename T>
    std::optional<T> InterpOpt(float t, const std::optional<T>& o0, const std::optional<T>& o1, const std::optional<T>& o2, const std::optional<T>& o3, InterpType type) {
        if (!o1 && !o2) return std::nullopt;
        T v1 = o1.value_or(o2.value_or(T{}));
        T v2 = o2.value_or(o1.value_or(T{}));
        T v0 = o0.value_or(v1);
        T v3 = o3.value_or(v2);
        return InterpVal(t, v0, v1, v2, v3, type);
    }

    static void InterpBloom(float t, const MoodBloomSettings& s0, const MoodBloomSettings& s1, const MoodBloomSettings& s2, const MoodBloomSettings& s3, MoodBloomSettings& res) {
        #define INTERP_B(member, type) res.member = InterpOpt(t, s0.member, s1.member, s2.member, s3.member, type)
        INTERP_B(toneMappingEnabled, InterpType::Boolean);
        INTERP_B(toneMappingMode, InterpType::Integer);
        INTERP_B(autoExposureEnabled, InterpType::Boolean);
        INTERP_B(targetLuminance, InterpType::Logarithmic);
        INTERP_B(minExposure, InterpType::Logarithmic);
        INTERP_B(maxExposure, InterpType::Logarithmic);
        INTERP_B(speedUp, InterpType::Linear);
        INTERP_B(speedDown, InterpType::Linear);
        INTERP_B(centerWeightTightness, InterpType::Linear);
        INTERP_B(focusPoint, InterpType::Linear);
        INTERP_B(histogramLowCutoff, InterpType::Linear);
        INTERP_B(histogramHighCutoff, InterpType::Linear);
        INTERP_B(uchimuraP, InterpType::Linear);
        INTERP_B(uchimuraA, InterpType::Linear);
        INTERP_B(uchimuraM, InterpType::Linear);
        INTERP_B(uchimuraL, InterpType::Linear);
        INTERP_B(uchimuraC, InterpType::Linear);
        INTERP_B(uchimuraB, InterpType::Linear);
        INTERP_B(autoTuneEnabled, InterpType::Boolean);
        INTERP_B(minContrast, InterpType::Linear);
        INTERP_B(maxContrast, InterpType::Linear);
        INTERP_B(targetBrightness, InterpType::Linear);
        INTERP_B(cdlSlope, InterpType::Linear);
        INTERP_B(cdlOffset, InterpType::Linear);
        INTERP_B(cdlPower, InterpType::Logarithmic);
        INTERP_B(cdlSaturation, InterpType::Linear);
        INTERP_B(whiteTemp, InterpType::Linear);
        INTERP_B(whiteTint, InterpType::Linear);
        INTERP_B(ltmEnabled, InterpType::Boolean);
        INTERP_B(ltmEvSpread, InterpType::Linear);
        INTERP_B(ltmTarget, InterpType::Linear);
        INTERP_B(ltmSigma, InterpType::Linear);
        INTERP_B(ltmWeightContrast, InterpType::Linear);
        INTERP_B(ltmWeightSaturation, InterpType::Linear);
        INTERP_B(ltmWeightExposedness, InterpType::Linear);
        INTERP_B(ltmBoostLocalContrast, InterpType::Linear);
        #undef INTERP_B
    }

    MoodSettings MoodManager::Interpolate(const MoodLayer& layer, float paramValue) {
        const auto& pts = layer.controlPoints;
        if (pts.empty()) return {};
        if (pts.size() == 1) return pts[0].settings;

        float wrap = 0.0f;
        bool isCyclic = false;
        switch(layer.trackedParameter) {
            case MoodParameter::TimeOfDay: wrap = 24.0f; isCyclic = true; break;
            case MoodParameter::MoonPhase: wrap = 2.0f; isCyclic = true; break;
            case MoodParameter::SunAngle:
            case MoodParameter::MoonAngle: wrap = 360.0f; isCyclic = true; break;
            default: break;
        }

        int i = 0;
        float t = 0.0f;
        int i0, i1, i2, i3;

        std::vector<MoodControlPoint> sortedPts = pts;
        std::sort(sortedPts.begin(), sortedPts.end(), [](const auto& a, const auto& b){
            return a.parameterValue < b.parameterValue;
        });

        if (isCyclic && wrap > 0.0f) {
            paramValue = std::fmod(paramValue, wrap);
            if (paramValue < 0) paramValue += wrap;

            while (i < (int)sortedPts.size() && sortedPts[i].parameterValue < paramValue) i++;

            i1 = (i == 0) ? (int)sortedPts.size() - 1 : i - 1;
            i2 = i % (int)sortedPts.size();
            i0 = (i1 == 0) ? (int)sortedPts.size() - 1 : i1 - 1;
            i3 = (i2 + 1) % (int)sortedPts.size();

            float v1 = sortedPts[i1].parameterValue;
            float v2 = sortedPts[i2].parameterValue;
            if (v2 < v1) v2 += wrap;
            float val = paramValue;
            if (val < v1) val += wrap;

            t = (val - v1) / (v2 - v1);
        } else {
            while (i < (int)sortedPts.size() - 1 && sortedPts[i+1].parameterValue < paramValue) i++;
            i1 = i;
            i2 = std::min((int)sortedPts.size() - 1, i + 1);
            i0 = std::max(0, i - 1);
            i3 = std::min((int)sortedPts.size() - 1, i + 2);
            if (sortedPts[i2].parameterValue != sortedPts[i1].parameterValue)
                t = (paramValue - sortedPts[i1].parameterValue) / (sortedPts[i2].parameterValue - sortedPts[i1].parameterValue);
        }
        t = glm::clamp(t, 0.0f, 1.0f);

        const MoodSettings &s0 = sortedPts[i0].settings, &s1 = sortedPts[i1].settings, &s2 = sortedPts[i2].settings, &s3 = sortedPts[i3].settings;
        MoodSettings res;
        InterpBloom(t, s0.sceneBloom, s1.sceneBloom, s2.sceneBloom, s3.sceneBloom, res.sceneBloom);
        InterpBloom(t, s0.skyBloom, s1.skyBloom, s2.skyBloom, s3.skyBloom, res.skyBloom);

        #define INTERP_S(member, type) res.member = InterpOpt(t, s0.member, s1.member, s2.member, s3.member, type)
        INTERP_S(cloudDensity, InterpType::Logarithmic);
        INTERP_S(cloudAltitude, InterpType::Linear);
        INTERP_S(cloudThickness, InterpType::Linear);
        INTERP_S(cloudColor, InterpType::Oklab);
        INTERP_S(cloudCoverage, InterpType::Linear);
        INTERP_S(cloudSunLightScale, InterpType::Logarithmic);
        INTERP_S(cloudMoonLightScale, InterpType::Logarithmic);
        INTERP_S(cloudPowderScale, InterpType::Logarithmic);
        INTERP_S(cloudBeerPowderMix, InterpType::Linear);
        INTERP_S(rayleighScale, InterpType::Logarithmic);
        INTERP_S(mieScale, InterpType::Logarithmic);
        INTERP_S(rayleighScattering, InterpType::Oklab);
        INTERP_S(mieScattering, InterpType::Logarithmic);
        INTERP_S(mieExtinction, InterpType::Logarithmic);
        #undef INTERP_S
        return res;
    }

    template<typename T>
    void BlendVal(std::optional<T>& base, const std::optional<T>& layer, MoodBlendMode mode) {
        if (!layer) return;
        if (!base) { base = layer; return; }
        switch(mode) {
            case MoodBlendMode::Add: *base += *layer; break;
            case MoodBlendMode::Subtract: *base -= *layer; break;
            case MoodBlendMode::Multiply: *base *= *layer; break;
            case MoodBlendMode::Divide: *base /= glm::max(*layer, T(1e-6f)); break;
            case MoodBlendMode::Override: *base = *layer; break;
        }
    }

    static void BlendBloom(MoodBloomSettings& base, const MoodBloomSettings& layer, MoodBlendMode mode) {
        #define BLEND_B(member) BlendVal(base.member, layer.member, mode)
        BLEND_B(toneMappingEnabled); BLEND_B(toneMappingMode); BLEND_B(autoExposureEnabled);
        BLEND_B(targetLuminance); BLEND_B(minExposure); BLEND_B(maxExposure);
        BLEND_B(speedUp); BLEND_B(speedDown); BLEND_B(centerWeightTightness);
        BLEND_B(focusPoint); BLEND_B(histogramLowCutoff); BLEND_B(histogramHighCutoff);
        BLEND_B(uchimuraP); BLEND_B(uchimuraA); BLEND_B(uchimuraM);
        BLEND_B(uchimuraL); BLEND_B(uchimuraC); BLEND_B(uchimuraB);
        BLEND_B(autoTuneEnabled); BLEND_B(minContrast); BLEND_B(maxContrast);
        BLEND_B(targetBrightness); BLEND_B(cdlSlope); BLEND_B(cdlOffset);
        BLEND_B(cdlPower); BLEND_B(cdlSaturation); BLEND_B(whiteTemp);
        BLEND_B(whiteTint); BLEND_B(ltmEnabled); BLEND_B(ltmEvSpread);
        BLEND_B(ltmTarget); BLEND_B(ltmSigma); BLEND_B(ltmWeightContrast);
        BLEND_B(ltmWeightSaturation); BLEND_B(ltmWeightExposedness); BLEND_B(ltmBoostLocalContrast);
        #undef BLEND_B
    }

    void MoodManager::Blend(MoodSettings& base, const MoodSettings& layer, MoodBlendMode mode) {
        BlendBloom(base.sceneBloom, layer.sceneBloom, mode);
        BlendBloom(base.skyBloom, layer.skyBloom, mode);
        #define BLEND_S(member) BlendVal(base.member, layer.member, mode)
        BLEND_S(cloudDensity); BLEND_S(cloudAltitude); BLEND_S(cloudThickness);
        BLEND_S(cloudColor); BLEND_S(cloudCoverage); BLEND_S(cloudSunLightScale);
        BLEND_S(cloudMoonLightScale); BLEND_S(cloudPowderScale); BLEND_S(cloudBeerPowderMix);
        BLEND_S(rayleighScale); BLEND_S(mieScale); BLEND_S(rayleighScattering);
        BLEND_S(mieScattering); BLEND_S(mieExtinction);
        #undef BLEND_S
    }

    template<typename T>
    void SmoothVal(std::optional<T>& current, const std::optional<T>& target, float deltaTime, float factor, InterpType type = InterpType::Linear) {
        if (!target) return;
        if (!current) { current = target; return; }

        float t = glm::clamp(deltaTime * factor, 0.0f, 1.0f);
        switch(type) {
            case InterpType::Logarithmic: {
                auto lCurr = glm::log(glm::max(*current, T(1e-6f)));
                auto lTarg = glm::log(glm::max(*target, T(1e-6f)));
                *current = glm::exp(glm::mix(lCurr, lTarg, t));
                break;
            }
            case InterpType::Oklab: {
                if constexpr (std::is_same_v<T, glm::vec3>) {
                    auto oCurr = LinearToOklab(*current);
                    auto oTarg = LinearToOklab(*target);
                    *current = OklabToLinear({
                        glm::mix(oCurr.L, oTarg.L, t),
                        glm::mix(oCurr.a, oTarg.a, t),
                        glm::mix(oCurr.b, oTarg.b, t)
                    });
                } else {
                    *current = glm::mix(*current, *target, t);
                }
                break;
            }
            case InterpType::Boolean:
            case InterpType::Integer:
                *current = *target;
                break;
            case InterpType::Linear:
            default:
                *current = glm::mix(*current, *target, t);
                break;
        }
    }

    static void SmoothBloom(MoodBloomSettings& current, const MoodBloomSettings& target, float deltaTime, float factor) {
        #define SMOOTH_B(member, type) SmoothVal(current.member, target.member, deltaTime, factor, type)
        SMOOTH_B(toneMappingEnabled, InterpType::Boolean);
        SMOOTH_B(toneMappingMode, InterpType::Integer);
        SMOOTH_B(autoExposureEnabled, InterpType::Boolean);
        SMOOTH_B(targetLuminance, InterpType::Logarithmic);
        SMOOTH_B(minExposure, InterpType::Logarithmic);
        SMOOTH_B(maxExposure, InterpType::Logarithmic);
        SMOOTH_B(speedUp, InterpType::Linear);
        SMOOTH_B(speedDown, InterpType::Linear);
        SMOOTH_B(centerWeightTightness, InterpType::Linear);
        SMOOTH_B(focusPoint, InterpType::Linear);
        SMOOTH_B(histogramLowCutoff, InterpType::Linear);
        SMOOTH_B(histogramHighCutoff, InterpType::Linear);
        SMOOTH_B(uchimuraP, InterpType::Linear);
        SMOOTH_B(uchimuraA, InterpType::Linear);
        SMOOTH_B(uchimuraM, InterpType::Linear);
        SMOOTH_B(uchimuraL, InterpType::Linear);
        SMOOTH_B(uchimuraC, InterpType::Linear);
        SMOOTH_B(uchimuraB, InterpType::Linear);
        SMOOTH_B(autoTuneEnabled, InterpType::Boolean);
        SMOOTH_B(minContrast, InterpType::Linear);
        SMOOTH_B(maxContrast, InterpType::Linear);
        SMOOTH_B(targetBrightness, InterpType::Linear);
        SMOOTH_B(cdlSlope, InterpType::Linear);
        SMOOTH_B(cdlOffset, InterpType::Linear);
        SMOOTH_B(cdlPower, InterpType::Logarithmic);
        SMOOTH_B(cdlSaturation, InterpType::Linear);
        SMOOTH_B(whiteTemp, InterpType::Linear);
        SMOOTH_B(whiteTint, InterpType::Linear);
        SMOOTH_B(ltmEnabled, InterpType::Boolean);
        SMOOTH_B(ltmEvSpread, InterpType::Linear);
        SMOOTH_B(ltmTarget, InterpType::Linear);
        SMOOTH_B(ltmSigma, InterpType::Linear);
        SMOOTH_B(ltmWeightContrast, InterpType::Linear);
        SMOOTH_B(ltmWeightSaturation, InterpType::Linear);
        SMOOTH_B(ltmWeightExposedness, InterpType::Linear);
        SMOOTH_B(ltmBoostLocalContrast, InterpType::Linear);
        #undef SMOOTH_B
    }

    void MoodManager::Smooth(MoodSettings& current, const MoodSettings& target, float deltaTime) {
        SmoothBloom(current.sceneBloom, target.sceneBloom, deltaTime, _smoothingFactor);
        SmoothBloom(current.skyBloom, target.skyBloom, deltaTime, _smoothingFactor);
        #define SMOOTH_S(member, type) SmoothVal(current.member, target.member, deltaTime, _smoothingFactor, type)
        SMOOTH_S(cloudDensity, InterpType::Logarithmic);
        SMOOTH_S(cloudAltitude, InterpType::Linear);
        SMOOTH_S(cloudThickness, InterpType::Linear);
        SMOOTH_S(cloudColor, InterpType::Oklab);
        SMOOTH_S(cloudCoverage, InterpType::Linear);
        SMOOTH_S(cloudSunLightScale, InterpType::Logarithmic);
        SMOOTH_S(cloudMoonLightScale, InterpType::Logarithmic);
        SMOOTH_S(cloudPowderScale, InterpType::Logarithmic);
        SMOOTH_S(cloudBeerPowderMix, InterpType::Linear);
        SMOOTH_S(rayleighScale, InterpType::Logarithmic);
        SMOOTH_S(mieScale, InterpType::Logarithmic);
        SMOOTH_S(rayleighScattering, InterpType::Oklab);
        SMOOTH_S(mieScattering, InterpType::Logarithmic);
        SMOOTH_S(mieExtinction, InterpType::Logarithmic);
        #undef SMOOTH_S
    }

} // namespace Boidsish
