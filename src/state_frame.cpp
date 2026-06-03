#include "state_frame.h"
#include "state.h"
#include "mood_manager.h"

namespace Boidsish {
namespace state {

	FrameBuffer::FrameBuffer() {
		for (auto& frame : frames_) {
			frame.user_input = new SystemConfiguration();
			frame.effective = new SystemConfiguration();
		}
	}

	FrameBuffer::~FrameBuffer() {
		for (auto& frame : frames_) {
			delete frame.user_input;
			delete frame.effective;
		}
	}

	void FrameBuffer::Swap() {
		std::swap(read_idx_, write_idx_);
		// Carry forward: new write side starts as a copy of committed read
		*frames_[write_idx_].user_input = *frames_[read_idx_].user_input;
		*frames_[write_idx_].effective = *frames_[read_idx_].effective;
		frames_[write_idx_].simulation = frames_[read_idx_].simulation;
		frames_[write_idx_].gen = frames_[read_idx_].gen;
		frames_[write_idx_].commands.Clear();
	}

	void MergeEffective(FrameBuffer& fb, const ::Boidsish::MoodSettings& mood) {
		auto& frame = fb.Write();
		const auto& prev_effective = *fb.Read().effective;

		// Start from user input
		*frame.effective = *frame.user_input;

		// Overlay mood onto atmosphere fields where it has opinions
		auto& atm = frame.effective->atmosphere;
		if (mood.cloudDensity) atm.cloudDensity = *mood.cloudDensity;
		if (mood.cloudAltitude) atm.cloudAltitude = *mood.cloudAltitude;
		if (mood.cloudThickness) atm.cloudThickness = *mood.cloudThickness;
		if (mood.cloudColor) atm.cloudColor = *mood.cloudColor;
		if (mood.cloudCoverage) atm.cloudCoverage = *mood.cloudCoverage;
		if (mood.cloudSunLightScale) atm.cloudSunLightScale = *mood.cloudSunLightScale;
		if (mood.cloudMoonLightScale) atm.cloudMoonLightScale = *mood.cloudMoonLightScale;
		if (mood.cloudPowderScale) atm.cloudPowderScale = *mood.cloudPowderScale;
		if (mood.cloudBeerPowderMix) atm.cloudBeerPowderMix = *mood.cloudBeerPowderMix;
		if (mood.rayleighScale) atm.rayleighScale = *mood.rayleighScale;
		if (mood.mieScale) atm.mieScale = *mood.mieScale;
		if (mood.rayleighScattering) atm.rayleighScattering = *mood.rayleighScattering;
		if (mood.mieScattering) atm.mieScattering = *mood.mieScattering;
		if (mood.mieExtinction) atm.mieExtinction = *mood.mieExtinction;

		// Overlay mood onto bloom fields
		auto applyBloomOverlay = [](BloomLayerSettings& layer, const ::Boidsish::MoodBloomSettings& mood_bloom) {
			if (mood_bloom.toneMappingEnabled) layer.toneMappingEnabled = *mood_bloom.toneMappingEnabled;
			if (mood_bloom.toneMappingMode) layer.toneMappingMode = *mood_bloom.toneMappingMode;
			if (mood_bloom.autoExposureEnabled) layer.autoExposureEnabled = *mood_bloom.autoExposureEnabled;
			if (mood_bloom.targetLuminance) layer.targetLuminance = *mood_bloom.targetLuminance;
			if (mood_bloom.minExposure) layer.minExposure = *mood_bloom.minExposure;
			if (mood_bloom.maxExposure) layer.maxExposure = *mood_bloom.maxExposure;
			if (mood_bloom.speedUp) layer.speedUp = *mood_bloom.speedUp;
			if (mood_bloom.speedDown) layer.speedDown = *mood_bloom.speedDown;
			if (mood_bloom.centerWeightTightness) layer.centerWeightTightness = *mood_bloom.centerWeightTightness;
			if (mood_bloom.focusPoint) layer.focusPoint = *mood_bloom.focusPoint;
			if (mood_bloom.histogramLowCutoff) layer.histogramLowCutoff = *mood_bloom.histogramLowCutoff;
			if (mood_bloom.histogramHighCutoff) layer.histogramHighCutoff = *mood_bloom.histogramHighCutoff;
			if (mood_bloom.uchimuraP) layer.uchimuraP = *mood_bloom.uchimuraP;
			if (mood_bloom.uchimuraA) layer.uchimuraA = *mood_bloom.uchimuraA;
			if (mood_bloom.uchimuraM) layer.uchimuraM = *mood_bloom.uchimuraM;
			if (mood_bloom.uchimuraL) layer.uchimuraL = *mood_bloom.uchimuraL;
			if (mood_bloom.uchimuraC) layer.uchimuraC = *mood_bloom.uchimuraC;
			if (mood_bloom.uchimuraB) layer.uchimuraB = *mood_bloom.uchimuraB;
			if (mood_bloom.autoTuneEnabled) layer.autoTuneEnabled = *mood_bloom.autoTuneEnabled;
			if (mood_bloom.minContrast) layer.minContrast = *mood_bloom.minContrast;
			if (mood_bloom.maxContrast) layer.maxContrast = *mood_bloom.maxContrast;
			if (mood_bloom.targetBrightness) layer.targetBrightness = *mood_bloom.targetBrightness;
			if (mood_bloom.cdlSlope) layer.cdlSlope = *mood_bloom.cdlSlope;
			if (mood_bloom.cdlOffset) layer.cdlOffset = *mood_bloom.cdlOffset;
			if (mood_bloom.cdlPower) layer.cdlPower = *mood_bloom.cdlPower;
			if (mood_bloom.cdlSaturation) layer.cdlSaturation = *mood_bloom.cdlSaturation;
			if (mood_bloom.whiteTemp) layer.whiteTemp = *mood_bloom.whiteTemp;
			if (mood_bloom.whiteTint) layer.whiteTint = *mood_bloom.whiteTint;
			if (mood_bloom.ltmEnabled) layer.ltmEnabled = *mood_bloom.ltmEnabled;
			if (mood_bloom.ltmEvSpread) layer.ltmEvSpread = *mood_bloom.ltmEvSpread;
			if (mood_bloom.ltmTarget) layer.ltmTarget = *mood_bloom.ltmTarget;
			if (mood_bloom.ltmSigma) layer.ltmSigma = *mood_bloom.ltmSigma;
			if (mood_bloom.ltmWeightContrast) layer.ltmWeightContrast = *mood_bloom.ltmWeightContrast;
			if (mood_bloom.ltmWeightSaturation) layer.ltmWeightSaturation = *mood_bloom.ltmWeightSaturation;
			if (mood_bloom.ltmWeightExposedness) layer.ltmWeightExposedness = *mood_bloom.ltmWeightExposedness;
			if (mood_bloom.ltmBoostLocalContrast) layer.ltmBoostLocalContrast = *mood_bloom.ltmBoostLocalContrast;
		};

		applyBloomOverlay(frame.effective->bloom.scene, mood.sceneBloom);
		applyBloomOverlay(frame.effective->bloom.sky, mood.skyBloom);

		// Bump generation counters where effective differs from previous frame
		if (!(frame.effective->grass == prev_effective.grass)) frame.gen.grass++;
		if (!(frame.effective->weather == prev_effective.weather)) frame.gen.weather++;
		if (!(frame.effective->atmosphere == prev_effective.atmosphere)) frame.gen.atmosphere++;
		if (!(frame.effective->dayNight == prev_effective.dayNight)) frame.gen.dayNight++;
		if (!(frame.effective->particles == prev_effective.particles)) frame.gen.particles++;
		if (!(frame.effective->terrain == prev_effective.terrain)) frame.gen.terrain++;
		if (!(frame.effective->volumetric == prev_effective.volumetric)) frame.gen.volumetric++;
		if (!(frame.effective->erosion == prev_effective.erosion)) frame.gen.erosion++;
		if (!(frame.effective->bloom == prev_effective.bloom)) frame.gen.bloom++;
		if (!(frame.effective->mood == prev_effective.mood)) frame.gen.mood++;
	}

} // namespace state
} // namespace Boidsish
