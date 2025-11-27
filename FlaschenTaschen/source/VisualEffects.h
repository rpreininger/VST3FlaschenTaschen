//------------------------------------------------------------------------
// Copyright(c) 2024 Stratojets.
//------------------------------------------------------------------------

#pragma once

#include "FlaschenTaschenClient.h"
#include "MappingConfig.h"
#include <chrono>
#include <random>
#include <array>

namespace FlaschenTaschen {

//------------------------------------------------------------------------
// PolyLightOrgan - polyphonic light organ mode
// Maps 61 keys (C1=24 to C6=84) across 128 pixel columns
// Each key lights up ~2 pixel wide vertical line
// Velocity/aftertouch controls brightness per key
//------------------------------------------------------------------------
class PolyLightOrgan {
public:
    static constexpr int MIN_NOTE = 24;   // C1
    static constexpr int MAX_NOTE = 84;   // C6
    static constexpr int NUM_KEYS = 61;   // 5 octaves

    PolyLightOrgan();

    // Note on with velocity (0-127)
    void noteOn(int midiNote, int velocity);

    // Note off
    void noteOff(int midiNote);

    // Update aftertouch for a note (-1 for channel aftertouch = all notes)
    void aftertouch(int midiNote, int pressure);

    // Set base color (can be changed dynamically)
    void setColor(uint8_t r, uint8_t g, uint8_t b);

    // Set rainbow mode (each key has different hue)
    void setRainbowMode(bool enabled) { rainbowMode_ = enabled; }
    bool isRainbowMode() const { return rainbowMode_; }

    // Render to display
    void render(FlaschenTaschenClient& client);

    // Check if any notes are active
    bool hasActiveNotes() const;

    // Clear all notes
    void allNotesOff();

private:
    struct KeyState {
        bool active = false;
        float brightness = 0.0f;  // 0.0 - 1.0
    };

    std::array<KeyState, 128> keys_;  // Full MIDI range for safety
    uint8_t baseR_ = 255, baseG_ = 255, baseB_ = 255;
    bool rainbowMode_ = true;  // Default to rainbow

    // Get pixel column range for a MIDI note
    void getNotePixelRange(int midiNote, int displayWidth, int& startX, int& endX) const;

    // HSV to RGB helper
    static Color hsvToRgb(float h, float s, float v);
};

//------------------------------------------------------------------------
// VisualEffects - renders visual effects on FlaschenTaschen display
//------------------------------------------------------------------------
class VisualEffects {
public:
    VisualEffects();
    ~VisualEffects() = default;

    // Start playing an effect
    void startEffect(const Effect& effect);

    // Start playing an effect with initial brightness (0-127 velocity mapped to 0.0-1.0)
    void startEffect(const Effect& effect, int velocity);

    // Stop current effect
    void stopEffect();

    // Check if an effect is currently playing
    bool isPlaying() const { return isPlaying_; }

    // Set brightness (0.0 - 1.0) - can be updated in real-time via aftertouch
    void setBrightness(float brightness);
    float getBrightness() const { return brightness_; }

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
    float brightness_ = 1.0f;  // 0.0 - 1.0, controlled by velocity/aftertouch
    std::chrono::steady_clock::time_point startTime_;
    std::mt19937 rng_;

    // Apply brightness to a color
    Color applyBrightness(const Color& c) const;

    // Animated effect renderers
    void renderPulse(FlaschenTaschenClient& client, float t);
    void renderFlash(FlaschenTaschenClient& client, float t);
    void renderStrobe(FlaschenTaschenClient& client, float t);
    void renderWave(FlaschenTaschenClient& client, float t);
    void renderSparkle(FlaschenTaschenClient& client, float t);
    void renderAnimatedRainbow(FlaschenTaschenClient& client, float t);

    // Non-static versions that apply brightness (for use in update())
    void renderColorRampWithBrightness(FlaschenTaschenClient& client,
                                       uint8_t r1, uint8_t g1, uint8_t b1,
                                       uint8_t r2, uint8_t g2, uint8_t b2,
                                       RampDirection direction);
    void renderAnimatedRainbowWithBrightness(FlaschenTaschenClient& client, float t);

    // Helper: interpolate between two colors
    static Color lerpColor(const Color& c1, const Color& c2, float t);

    // Helper: HSV to RGB conversion
    static Color hsvToRgb(float h, float s, float v);
};

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
