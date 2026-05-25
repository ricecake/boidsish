#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <glm/glm.hpp>
#include "post_processing/effects/BloomEffect.h"

namespace Boidsish {

    enum class MoodBlendMode {
        Add,
        Subtract,
        Override,
        Multiply,
        Divide
    };

    enum class MoodParameter {
        TimeOfDay,
        Precipitation,
        Temperature,
        CloudCover,
        MoonAngle,
        SunAngle,
        MoonPhase,
        WorldPositionX,
        WorldPositionY,
        WorldPositionZ,
        Count
    };

    struct MoodBloomSettings {
        std::optional<bool> toneMappingEnabled;
        std::optional<int> toneMappingMode;
        std::optional<bool> autoExposureEnabled;
        std::optional<float> targetLuminance;
        std::optional<float> minExposure;
        std::optional<float> maxExposure;
        std::optional<float> speedUp;
        std::optional<float> speedDown;
        std::optional<float> centerWeightTightness;
        std::optional<glm::vec2> focusPoint;
        std::optional<float> histogramLowCutoff;
        std::optional<float> histogramHighCutoff;
        std::optional<float> uchimuraP;
        std::optional<float> uchimuraA;
        std::optional<float> uchimuraM;
        std::optional<float> uchimuraL;
        std::optional<float> uchimuraC;
        std::optional<float> uchimuraB;
        std::optional<bool> autoTuneEnabled;
        std::optional<float> minContrast;
        std::optional<float> maxContrast;
        std::optional<float> targetBrightness;
        std::optional<glm::vec3> cdlSlope;
        std::optional<glm::vec3> cdlOffset;
        std::optional<glm::vec3> cdlPower;
        std::optional<float> cdlSaturation;
        std::optional<float> whiteTemp;
        std::optional<float> whiteTint;
        std::optional<bool> ltmEnabled;
        std::optional<float> ltmEvSpread;
        std::optional<float> ltmTarget;
        std::optional<float> ltmSigma;
        std::optional<float> ltmWeightContrast;
        std::optional<float> ltmWeightSaturation;
        std::optional<float> ltmWeightExposedness;
        std::optional<float> ltmBoostLocalContrast;
    };

    struct MoodSettings {
        MoodBloomSettings sceneBloom;
        MoodBloomSettings skyBloom;

        // Cloud / Atmosphere parameters
        std::optional<float> cloudDensity;
        std::optional<float> cloudAltitude;
        std::optional<float> cloudThickness;
        std::optional<glm::vec3> cloudColor;
        std::optional<float> cloudCoverage;
        std::optional<float> cloudSunLightScale;
        std::optional<float> cloudMoonLightScale;
        std::optional<float> cloudPowderScale;
        std::optional<float> cloudBeerPowderMix;

        // Atmosphere scattering
        std::optional<float> rayleighScale;
        std::optional<float> mieScale;
        std::optional<glm::vec3> rayleighScattering;
        std::optional<float> mieScattering;
        std::optional<float> mieExtinction;
    };

    struct MoodControlPoint {
        float parameterValue;
        MoodSettings settings;
    };

    struct MoodLayer {
        std::string name;
        int priority;
        MoodBlendMode blendMode;
        MoodParameter trackedParameter;
        std::vector<MoodControlPoint> controlPoints;
        bool enabled = true;
    };

    class MoodManager {
    public:
        MoodManager();
        ~MoodManager();

        void Update(const std::map<MoodParameter, float>& currentParams);

        void AddLayer(const MoodLayer& layer);
        void RemoveLayer(const std::string& name);
        void SetLayerEnabled(const std::string& name, bool enabled);

        const MoodSettings& GetBlendedSettings() const { return _blendedSettings; }

        // User overrides
        void SetOverride(const MoodSettings& settings, bool enabled);
        bool IsOverrideEnabled() const { return _overrideEnabled; }

    private:
        MoodSettings Interpolate(const MoodLayer& layer, float paramValue);
        void Blend(MoodSettings& base, const MoodSettings& layer, MoodBlendMode mode);

        std::vector<MoodLayer> _layers;
        MoodSettings _blendedSettings;

        MoodSettings _userOverride;
        bool _overrideEnabled = false;

        std::map<MoodParameter, float> _currentParams;
    };

} // namespace Boidsish
