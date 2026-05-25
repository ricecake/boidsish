#pragma once

#include <string>
#include <vector>
#include <map>
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

    struct MoodSettings {
        PostProcessing::BloomEffect::LayerSettings sceneBloom;
        PostProcessing::BloomEffect::LayerSettings skyBloom;

        // Cloud / Atmosphere parameters
        float cloudDensity;
        float cloudAltitude;
        float cloudThickness;
        glm::vec3 cloudColor;
        float cloudCoverage;
        float cloudSunLightScale;
        float cloudMoonLightScale;
        float cloudPowderScale;
        float cloudBeerPowderMix;

        // Atmosphere scattering
        float rayleighScale;
        float mieScale;
        glm::vec3 rayleighScattering;
        float mieScattering;
        float mieExtinction;
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
