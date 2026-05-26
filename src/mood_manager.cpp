#include "mood_manager.h"
#include <algorithm>
#include <cmath>
#include <set>
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

    // Centripetal Catmull-Rom (Aitken's Algorithm)
    template<typename T>
    static T CentripetalCatmullRom(float val, T p0, T p1, T p2, T p3, float t0, float t1, float t2, float t3) {
        if (t1 == t2) return p1;

        auto a1 = p0 * ((t1 - val) / (t1 - t0)) + p1 * ((val - t0) / (t1 - t0));
        auto a2 = p1 * ((t2 - val) / (t2 - t1)) + p2 * ((val - t1) / (t2 - t1));
        auto a3 = p2 * ((t3 - val) / (t3 - t2)) + p3 * ((val - t2) / (t3 - t2));

        auto b1 = a1 * ((t2 - val) / (t2 - t0)) + a2 * ((val - t0) / (t2 - t0));
        auto b2 = a2 * ((t3 - val) / (t3 - t1)) + a3 * ((val - t1) / (t3 - t1));

        return b1 * ((t2 - val) / (t2 - t1)) + b2 * ((val - t1) / (t2 - t1));
    }

    MoodManager::MoodManager() {}
    MoodManager::~MoodManager() {}

    enum class InterpType { Linear, Logarithmic, Oklab, Slerp, Boolean, Integer };

    template<typename T>
    static T LerpVal(float t, T v1, T v2, InterpType type) {
        switch(type) {
            case InterpType::Logarithmic: {
                auto l1 = glm::log(glm::max(v1, T(1e-6f)));
                auto l2 = glm::log(glm::max(v2, T(1e-6f)));
                return glm::exp(glm::mix(l1, l2, t));
            }
            case InterpType::Oklab: {
                if constexpr (std::is_same_v<T, glm::vec3>) {
                    auto o1 = LinearToOklab(v1);
                    auto o2 = LinearToOklab(v2);
                    return OklabToLinear({
                        glm::mix(o1.L, o2.L, t),
                        glm::mix(o1.a, o2.a, t),
                        glm::mix(o1.b, o2.b, t)
                    });
                }
                return glm::mix(v1, v2, t);
            }
            case InterpType::Slerp: {
                if constexpr (std::is_same_v<T, glm::vec3>) {
                    float dot = glm::dot(glm::normalize(v1), glm::normalize(v2));
                    if (dot > 0.9995f) return glm::mix(v1, v2, t);
                    float theta_0 = std::acos(glm::clamp(dot, -1.0f, 1.0f));
                    float theta = theta_0 * t;
                    glm::vec3 v3 = glm::normalize(v2 - v1 * dot);
                    return (v1 * std::cos(theta) + v3 * std::sin(theta)) * glm::mix(glm::length(v1), glm::length(v2), t);
                }
                return glm::mix(v1, v2, t);
            }
            case InterpType::Boolean:
            case InterpType::Integer: return (t < 0.5f) ? v1 : v2;
            case InterpType::Linear:
            default: return glm::mix(v1, v2, t);
        }
    }

    static void FillBloomHoles(std::vector<MoodControlPoint>& pts) {
        #define FILL_B(member, type) { \
            bool any = false; for (const auto& p : pts) if (p.settings.sceneBloom.member || p.settings.skyBloom.member) { any = true; break; } \
            if (any) { \
                for (size_t i = 0; i < pts.size(); ++i) { \
                    if (!pts[i].settings.sceneBloom.member) { \
                        int prev = -1, next = -1; \
                        for (int j = (int)i - 1; j >= 0; --j) if (pts[j].settings.sceneBloom.member) { prev = j; break; } \
                        for (int j = (int)i + 1; j < (int)pts.size(); ++j) if (pts[j].settings.sceneBloom.member) { next = j; break; } \
                        if (prev != -1 && next != -1) { \
                            float t = (pts[i].parameterValue - pts[prev].parameterValue) / (pts[next].parameterValue - pts[prev].parameterValue); \
                            pts[i].settings.sceneBloom.member = LerpVal(t, *pts[prev].settings.sceneBloom.member, *pts[next].settings.sceneBloom.member, type); \
                        } else if (prev != -1) pts[i].settings.sceneBloom.member = pts[prev].settings.sceneBloom.member; \
                        else if (next != -1) pts[i].settings.sceneBloom.member = pts[next].settings.sceneBloom.member; \
                    } \
                    if (!pts[i].settings.skyBloom.member) { \
                        int prev = -1, next = -1; \
                        for (int j = (int)i - 1; j >= 0; --j) if (pts[j].settings.skyBloom.member) { prev = j; break; } \
                        for (int j = (int)i + 1; j < (int)pts.size(); ++j) if (pts[j].settings.skyBloom.member) { next = j; break; } \
                        if (prev != -1 && next != -1) { \
                            float t = (pts[i].parameterValue - pts[prev].parameterValue) / (pts[next].parameterValue - pts[prev].parameterValue); \
                            pts[i].settings.skyBloom.member = LerpVal(t, *pts[prev].settings.skyBloom.member, *pts[next].settings.skyBloom.member, type); \
                        } else if (prev != -1) pts[i].settings.skyBloom.member = pts[prev].settings.skyBloom.member; \
                        else if (next != -1) pts[i].settings.skyBloom.member = pts[next].settings.skyBloom.member; \
                    } \
                } \
            } \
        }
        FILL_B(toneMappingEnabled, InterpType::Boolean);
        FILL_B(toneMappingMode, InterpType::Integer);
        FILL_B(autoExposureEnabled, InterpType::Boolean);
        FILL_B(targetLuminance, InterpType::Logarithmic);
        FILL_B(minExposure, InterpType::Logarithmic);
        FILL_B(maxExposure, InterpType::Logarithmic);
        FILL_B(speedUp, InterpType::Linear);
        FILL_B(speedDown, InterpType::Linear);
        FILL_B(centerWeightTightness, InterpType::Linear);
        FILL_B(focusPoint, InterpType::Linear);
        FILL_B(histogramLowCutoff, InterpType::Linear);
        FILL_B(histogramHighCutoff, InterpType::Linear);
        FILL_B(uchimuraP, InterpType::Linear);
        FILL_B(uchimuraA, InterpType::Linear);
        FILL_B(uchimuraM, InterpType::Linear);
        FILL_B(uchimuraL, InterpType::Linear);
        FILL_B(uchimuraC, InterpType::Linear);
        FILL_B(uchimuraB, InterpType::Linear);
        FILL_B(autoTuneEnabled, InterpType::Boolean);
        FILL_B(minContrast, InterpType::Linear);
        FILL_B(maxContrast, InterpType::Linear);
        FILL_B(targetBrightness, InterpType::Linear);
        FILL_B(cdlSlope, InterpType::Linear);
        FILL_B(cdlOffset, InterpType::Linear);
        FILL_B(cdlPower, InterpType::Logarithmic);
        FILL_B(cdlSaturation, InterpType::Linear);
        FILL_B(whiteTemp, InterpType::Linear);
        FILL_B(whiteTint, InterpType::Linear);
        FILL_B(ltmEnabled, InterpType::Boolean);
        FILL_B(ltmEvSpread, InterpType::Linear);
        FILL_B(ltmTarget, InterpType::Linear);
        FILL_B(ltmSigma, InterpType::Linear);
        FILL_B(ltmWeightContrast, InterpType::Linear);
        FILL_B(ltmWeightSaturation, InterpType::Linear);
        FILL_B(ltmWeightExposedness, InterpType::Linear);
        FILL_B(ltmBoostLocalContrast, InterpType::Linear);
        #undef FILL_B
    }

    static void FillSettingsHoles(std::vector<MoodControlPoint>& pts) {
        FillBloomHoles(pts);
        #define FILL_S(member, type) { \
            bool any = false; for (const auto& p : pts) if (p.settings.member) { any = true; break; } \
            if (any) { \
                for (size_t i = 0; i < pts.size(); ++i) { \
                    if (!pts[i].settings.member) { \
                        int prev = -1, next = -1; \
                        for (int j = (int)i - 1; j >= 0; --j) if (pts[j].settings.member) { prev = j; break; } \
                        for (int j = (int)i + 1; j < (int)pts.size(); ++j) if (pts[j].settings.member) { next = j; break; } \
                        if (prev != -1 && next != -1) { \
                            float t = (pts[i].parameterValue - pts[prev].parameterValue) / (pts[next].parameterValue - pts[prev].parameterValue); \
                            pts[i].settings.member = LerpVal(t, *pts[prev].settings.member, *pts[next].settings.member, type); \
                        } else if (prev != -1) pts[i].settings.member = pts[prev].settings.member; \
                        else if (next != -1) pts[i].settings.member = pts[next].settings.member; \
                    } \
                } \
            } \
        }
        FILL_S(cloudDensity, InterpType::Logarithmic);
        FILL_S(cloudAltitude, InterpType::Linear);
        FILL_S(cloudThickness, InterpType::Linear);
        FILL_S(cloudColor, InterpType::Oklab);
        FILL_S(cloudCoverage, InterpType::Linear);
        FILL_S(cloudSunLightScale, InterpType::Logarithmic);
        FILL_S(cloudMoonLightScale, InterpType::Logarithmic);
        FILL_S(cloudPowderScale, InterpType::Logarithmic);
        FILL_S(cloudBeerPowderMix, InterpType::Linear);
        FILL_S(rayleighScale, InterpType::Logarithmic);
        FILL_S(mieScale, InterpType::Logarithmic);
        FILL_S(rayleighScattering, InterpType::Oklab);
        FILL_S(mieScattering, InterpType::Logarithmic);
        FILL_S(mieExtinction, InterpType::Logarithmic);
        #undef FILL_S
    }

    void MoodManager::Update(const std::map<MoodParameter, float>& currentParams, float deltaTime) {
        _currentParams = currentParams;
        _blendedSettings = {};

        std::vector<MoodLayer*> sortedLayers;
        for (auto& l : _layers) sortedLayers.push_back(&l);
        std::sort(sortedLayers.begin(), sortedLayers.end(), [](const auto* a, const auto* b) {
            return a->priority < b->priority;
        });

        for (auto* layer : sortedLayers) {
            if (!layer->enabled || layer->controlPoints.empty()) continue;

            float paramValue = 0.0f;
            if (_currentParams.count(layer->trackedParameter)) {
                paramValue = _currentParams.at(layer->trackedParameter);
            }

            MoodSettings layerContribution = Interpolate(*layer, paramValue);
            Blend(_blendedSettings, layerContribution, layer->blendMode);
        }

        if (_overrideEnabled) {
            _blendedSettings = _userOverride;
        }

        Smooth(_smoothedSettings, _blendedSettings, deltaTime);
    }

    void MoodManager::AddLayer(const MoodLayer& layer) {
        MoodLayer l = layer;
        std::sort(l.controlPoints.begin(), l.controlPoints.end(), [](const auto& a, const auto& b){
            return a.parameterValue < b.parameterValue;
        });
        FillSettingsHoles(l.controlPoints);
        _layers.push_back(l);
    }

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

    template<typename T>
    static T InterpValT(float val, T v0, T v1, T v2, T v3, InterpType type, float k0, float k1, float k2, float k3) {
        switch(type) {
            case InterpType::Logarithmic: {
                auto l0 = glm::log(glm::max(v0, T(1e-6f)));
                auto l1 = glm::log(glm::max(v1, T(1e-6f)));
                auto l2 = glm::log(glm::max(v2, T(1e-6f)));
                auto l3 = glm::log(glm::max(v3, T(1e-6f)));
                return glm::exp(CentripetalCatmullRom(val, l0, l1, l2, l3, k0, k1, k2, k3));
            }
            case InterpType::Oklab: {
                if constexpr (std::is_same_v<T, glm::vec3>) {
                    auto o0 = LinearToOklab(v0);
                    auto o1 = LinearToOklab(v1);
                    auto o2 = LinearToOklab(v2);
                    auto o3 = LinearToOklab(v3);
                    return OklabToLinear({
                        CentripetalCatmullRom(val, o0.L, o1.L, o2.L, o3.L, k0, k1, k2, k3),
                        CentripetalCatmullRom(val, o0.a, o1.a, o2.a, o3.a, k0, k1, k2, k3),
                        CentripetalCatmullRom(val, o0.b, o1.b, o2.b, o3.b, k0, k1, k2, k3)
                    });
                }
                return CentripetalCatmullRom(val, v0, v1, v2, v3, k0, k1, k2, k3);
            }
            case InterpType::Boolean:
            case InterpType::Integer: return (val < (k1 + 0.5f * (k2 - k1))) ? v1 : v2;
            case InterpType::Linear:
            default: return CentripetalCatmullRom(val, v0, v1, v2, v3, k0, k1, k2, k3);
        }
    }

    template<typename T>
    static std::optional<T> InterpOptT(float val, const std::optional<T>& o0, const std::optional<T>& o1, const std::optional<T>& o2, const std::optional<T>& o3, InterpType type, float k0, float k1, float k2, float k3) {
        if (!o1 && !o2) return std::nullopt;
        if (o1 && o2) return InterpValT(val, o0.value_or(*o1), *o1, *o2, o3.value_or(*o2), type, k0, k1, k2, k3);
        return o1 ? o1 : o2;
    }

    static void InterpBloom(float val, const MoodBloomSettings& s0, const MoodBloomSettings& s1, const MoodBloomSettings& s2, const MoodBloomSettings& s3, MoodBloomSettings& res, float k0, float k1, float k2, float k3) {
        #define INTERP_B(member, type) res.member = InterpOptT(val, s0.member, s1.member, s2.member, s3.member, type, k0, k1, k2, k3)
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
        int i0, i1, i2, i3;
        const auto& sortedPts = pts;

        if (isCyclic && wrap > 0.0f) {
            paramValue = std::fmod(paramValue, wrap);
            if (paramValue < 0) paramValue += wrap;
            while (i < (int)sortedPts.size() && sortedPts[i].parameterValue < paramValue) i++;
            i1 = (i == 0) ? (int)sortedPts.size() - 1 : i - 1;
            i2 = i % (int)sortedPts.size();
            i0 = (i1 == 0) ? (int)sortedPts.size() - 1 : i1 - 1;
            i3 = (i2 + 1) % (int)sortedPts.size();
        } else {
            while (i < (int)sortedPts.size() - 1 && sortedPts[i+1].parameterValue < paramValue) i++;
            i1 = i;
            i2 = std::min((int)sortedPts.size() - 1, i + 1);
            i0 = std::max(0, i - 1);
            i3 = std::min((int)sortedPts.size() - 1, i + 2);
        }

        float p0v = sortedPts[i0].parameterValue;
        float p1v = sortedPts[i1].parameterValue;
        float p2v = sortedPts[i2].parameterValue;
        float p3v = sortedPts[i3].parameterValue;

        if (isCyclic && wrap > 0.0f) {
            if (p1v > paramValue) p1v -= wrap;
            if (p0v > p1v) p0v -= wrap;
            if (p2v < paramValue) p2v += wrap;
            if (p3v < p2v) p3v += wrap;
        }

        float k1 = 0.0f;
        float k0 = -std::sqrt(std::abs(p1v - p0v));
        float k2 = std::sqrt(std::abs(p2v - p1v));
        float k3 = k2 + std::sqrt(std::abs(p3v - p2v));

        float t = 0.0f;
        if (p2v != p1v) t = (paramValue - p1v) / (p2v - p1v);
        t = glm::clamp(t, 0.0f, 1.0f);
        float val = t * k2;

        const MoodSettings &s0 = sortedPts[i0].settings, &s1 = sortedPts[i1].settings, &s2 = sortedPts[i2].settings, &s3 = sortedPts[i3].settings;
        MoodSettings res;
        InterpBloom(val, s0.sceneBloom, s1.sceneBloom, s2.sceneBloom, s3.sceneBloom, res.sceneBloom, k0, k1, k2, k3);
        InterpBloom(val, s0.skyBloom, s1.skyBloom, s2.skyBloom, s3.skyBloom, res.skyBloom, k0, k1, k2, k3);

        #define INTERP_S(member, type) res.member = InterpOptT(val, s0.member, s1.member, s2.member, s3.member, type, k0, k1, k2, k3)
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
        if (!current || factor <= 0.0f) { current = target; return; }

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

    struct TimeDrivenFlags {
        struct Bloom {
            bool toneMappingEnabled=0, toneMappingMode=0, autoExposureEnabled=0, targetLuminance=0, minExposure=0, maxExposure=0;
            bool speedUp=0, speedDown=0, centerWeightTightness=0, focusPoint=0, histogramLowCutoff=0, histogramHighCutoff=0;
            bool uchimuraP=0, uchimuraA=0, uchimuraM=0, uchimuraL=0, uchimuraC=0, uchimuraB=0, autoTuneEnabled=0;
            bool minContrast=0, maxContrast=0, targetBrightness=0, cdlSlope=0, cdlOffset=0, cdlPower=0, cdlSaturation=0;
            bool whiteTemp=0, whiteTint=0, ltmEnabled=0, ltmEvSpread=0, ltmTarget=0, ltmSigma=0, ltmWeightContrast=0;
            bool ltmWeightSaturation=0, ltmWeightExposedness=0, ltmBoostLocalContrast=0;
        } sceneBloom, skyBloom;
        bool cloudDensity=0, cloudAltitude=0, cloudThickness=0, cloudColor=0, cloudCoverage=0, cloudSunLightScale=0;
        bool cloudMoonLightScale=0, cloudPowderScale=0, cloudBeerPowderMix=0, rayleighScale=0, mieScale=0, rayleighScattering=0;
        bool mieScattering=0, mieExtinction=0;
    };

    void MoodManager::Smooth(MoodSettings& current, const MoodSettings& target, float deltaTime) {
        TimeDrivenFlags flags = {};
        for (const auto& layer : _layers) {
            if (!layer.enabled) continue;
            bool isTime = false;
            switch(layer.trackedParameter) {
                case MoodParameter::TimeOfDay: case MoodParameter::MoonPhase:
                case MoodParameter::SunAngle: case MoodParameter::MoonAngle: isTime = true; break;
                default: break;
            }
            if (isTime) {
                #define MARK_B(member) if (layer.controlPoints[0].settings.sceneBloom.member) flags.sceneBloom.member = true; \
                                       if (layer.controlPoints[0].settings.skyBloom.member) flags.skyBloom.member = true;
                MARK_B(toneMappingEnabled); MARK_B(toneMappingMode); MARK_B(autoExposureEnabled);
                MARK_B(targetLuminance); MARK_B(minExposure); MARK_B(maxExposure);
                MARK_B(speedUp); MARK_B(speedDown); MARK_B(centerWeightTightness);
                MARK_B(focusPoint); MARK_B(histogramLowCutoff); MARK_B(histogramHighCutoff);
                MARK_B(uchimuraP); MARK_B(uchimuraA); MARK_B(uchimuraM);
                MARK_B(uchimuraL); MARK_B(uchimuraC); MARK_B(uchimuraB);
                MARK_B(autoTuneEnabled); MARK_B(minContrast); MARK_B(maxContrast);
                MARK_B(targetBrightness); MARK_B(cdlSlope); MARK_B(cdlOffset);
                MARK_B(cdlPower); MARK_B(cdlSaturation); MARK_B(whiteTemp);
                MARK_B(whiteTint); MARK_B(ltmEnabled); MARK_B(ltmEvSpread);
                MARK_B(ltmTarget); MARK_B(ltmSigma); MARK_B(ltmWeightContrast);
                MARK_B(ltmWeightSaturation); MARK_B(ltmWeightExposedness); MARK_B(ltmBoostLocalContrast);
                #undef MARK_B
                #define MARK_S(member) if (layer.controlPoints[0].settings.member) flags.member = true;
                MARK_S(cloudDensity); MARK_S(cloudAltitude); MARK_S(cloudThickness);
                MARK_S(cloudColor); MARK_S(cloudCoverage); MARK_S(cloudSunLightScale);
                MARK_S(cloudMoonLightScale); MARK_S(cloudPowderScale); MARK_S(cloudBeerPowderMix);
                MARK_S(rayleighScale); MARK_S(mieScale); MARK_S(rayleighScattering);
                MARK_S(mieScattering); MARK_S(mieExtinction);
                #undef MARK_S
            }
        }

        #define SM_B(dest, src, flag, type) SmoothVal(dest, src, deltaTime, flag ? -1.0f : _smoothingFactor, type);

        #define SMOOTH_B_BLOCK(d_block, s_block, f_block) { \
            SM_B(d_block.toneMappingEnabled, s_block.toneMappingEnabled, f_block.toneMappingEnabled, InterpType::Boolean); \
            SM_B(d_block.toneMappingMode, s_block.toneMappingMode, f_block.toneMappingMode, InterpType::Integer); \
            SM_B(d_block.autoExposureEnabled, s_block.autoExposureEnabled, f_block.autoExposureEnabled, InterpType::Boolean); \
            SM_B(d_block.targetLuminance, s_block.targetLuminance, f_block.targetLuminance, InterpType::Logarithmic); \
            SM_B(d_block.minExposure, s_block.minExposure, f_block.minExposure, InterpType::Logarithmic); \
            SM_B(d_block.maxExposure, s_block.maxExposure, f_block.maxExposure, InterpType::Logarithmic); \
            SM_B(d_block.speedUp, s_block.speedUp, f_block.speedUp, InterpType::Linear); \
            SM_B(d_block.speedDown, s_block.speedDown, f_block.speedDown, InterpType::Linear); \
            SM_B(d_block.centerWeightTightness, s_block.centerWeightTightness, f_block.centerWeightTightness, InterpType::Linear); \
            SM_B(d_block.focusPoint, s_block.focusPoint, f_block.focusPoint, InterpType::Linear); \
            SM_B(d_block.histogramLowCutoff, s_block.histogramLowCutoff, f_block.histogramLowCutoff, InterpType::Linear); \
            SM_B(d_block.histogramHighCutoff, s_block.histogramHighCutoff, f_block.histogramHighCutoff, InterpType::Linear); \
            SM_B(d_block.uchimuraP, s_block.uchimuraP, f_block.uchimuraP, InterpType::Linear); \
            SM_B(d_block.uchimuraA, s_block.uchimuraA, f_block.uchimuraA, InterpType::Linear); \
            SM_B(d_block.uchimuraM, s_block.uchimuraM, f_block.uchimuraM, InterpType::Linear); \
            SM_B(d_block.uchimuraL, s_block.uchimuraL, f_block.uchimuraL, InterpType::Linear); \
            SM_B(d_block.uchimuraC, s_block.uchimuraC, f_block.uchimuraC, InterpType::Linear); \
            SM_B(d_block.uchimuraB, s_block.uchimuraB, f_block.uchimuraB, InterpType::Linear); \
            SM_B(d_block.autoTuneEnabled, s_block.autoTuneEnabled, f_block.autoTuneEnabled, InterpType::Boolean); \
            SM_B(d_block.minContrast, s_block.minContrast, f_block.minContrast, InterpType::Linear); \
            SM_B(d_block.maxContrast, s_block.maxContrast, f_block.maxContrast, InterpType::Linear); \
            SM_B(d_block.targetBrightness, s_block.targetBrightness, f_block.targetBrightness, InterpType::Linear); \
            SM_B(d_block.cdlSlope, s_block.cdlSlope, f_block.cdlSlope, InterpType::Linear); \
            SM_B(d_block.cdlOffset, s_block.cdlOffset, f_block.cdlOffset, InterpType::Linear); \
            SM_B(d_block.cdlPower, s_block.cdlPower, f_block.cdlPower, InterpType::Logarithmic); \
            SM_B(d_block.cdlSaturation, s_block.cdlSaturation, f_block.cdlSaturation, InterpType::Linear); \
            SM_B(d_block.whiteTemp, s_block.whiteTemp, f_block.whiteTemp, InterpType::Linear); \
            SM_B(d_block.whiteTint, s_block.whiteTint, f_block.whiteTint, InterpType::Linear); \
            SM_B(d_block.ltmEnabled, s_block.ltmEnabled, f_block.ltmEnabled, InterpType::Boolean); \
            SM_B(d_block.ltmEvSpread, s_block.ltmEvSpread, f_block.ltmEvSpread, InterpType::Linear); \
            SM_B(d_block.ltmTarget, s_block.ltmTarget, f_block.ltmTarget, InterpType::Linear); \
            SM_B(d_block.ltmSigma, s_block.ltmSigma, f_block.ltmSigma, InterpType::Linear); \
            SM_B(d_block.ltmWeightContrast, s_block.ltmWeightContrast, f_block.ltmWeightContrast, InterpType::Linear); \
            SM_B(d_block.ltmWeightSaturation, s_block.ltmWeightSaturation, f_block.ltmWeightSaturation, InterpType::Linear); \
            SM_B(d_block.ltmWeightExposedness, s_block.ltmWeightExposedness, f_block.ltmWeightExposedness, InterpType::Linear); \
            SM_B(d_block.ltmBoostLocalContrast, s_block.ltmBoostLocalContrast, f_block.ltmBoostLocalContrast, InterpType::Linear); \
        }

        SMOOTH_B_BLOCK(current.sceneBloom, target.sceneBloom, flags.sceneBloom);
        SMOOTH_B_BLOCK(current.skyBloom, target.skyBloom, flags.skyBloom);

        #define SMOOTH_S(member, type) SmoothVal(current.member, target.member, deltaTime, flags.member ? -1.0f : _smoothingFactor, type)
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
