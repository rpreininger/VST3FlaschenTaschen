//------------------------------------------------------------------------
// Copyright(c) 2024 Stratojets.
//------------------------------------------------------------------------

#pragma once

#include "FlaschenTaschenClient.h"
#include "MappingConfig.h"
#include <chrono>
#include <random>

namespace FlaschenTaschen {

//------------------------------------------------------------------------
// VisualEffects - renders visual effects on FlaschenTaschen display
//------------------------------------------------------------------------
class VisualEffects {
public:
    VisualEffects();
    ~VisualEffects() = default;

    // Start playing an effect
    void startEffect(const Effect& effect);

    // Stop current effect
    void stopEffect();

    // Check if an effect is currently playing
    bool isPlaying() const { return isPlaying_; }

    // Update and render current effect
    // Returns true if effect is still active, false if finished
    bool update(FlaschenTaschenClient& client);

    // Get elapsed time since effect started (ms)
    int getElapsedMs() const;

    // Static effect renderers (one-shot, no animation)
    static void renderSolidColor(FlaschenTaschenClient& client,
                                  uint8_t r, uint8_t g, uint8_t b);

    static void renderColorRamp(FlaschenTaschenClient& client,
                                 uint8_t r1, uint8_t g1, uint8_t b1,
                                 uint8_t r2, uint8_t g2, uint8_t b2,
                                 RampDirection direction);

    static void renderRainbow(FlaschenTaschenClient& client, float phase);

private:
    Effect currentEffect_;
    bool isPlaying_ = false;
    std::chrono::steady_clock::time_point startTime_;
    std::mt19937 rng_;

    // Animated effect renderers
    void renderPulse(FlaschenTaschenClient& client, float t);
    void renderFlash(FlaschenTaschenClient& client, float t);
    void renderStrobe(FlaschenTaschenClient& client, float t);
    void renderWave(FlaschenTaschenClient& client, float t);
    void renderSparkle(FlaschenTaschenClient& client, float t);
    void renderAnimatedRainbow(FlaschenTaschenClient& client, float t);

    // Helper: interpolate between two colors
    static Color lerpColor(const Color& c1, const Color& c2, float t);

    // Helper: HSV to RGB conversion
    static Color hsvToRgb(float h, float s, float v);
};

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
