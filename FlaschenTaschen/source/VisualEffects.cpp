//------------------------------------------------------------------------
// Copyright(c) 2024 Stratojets.
//------------------------------------------------------------------------

#include "VisualEffects.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace FlaschenTaschen {

//------------------------------------------------------------------------
VisualEffects::VisualEffects()
    : rng_(std::random_device{}())
{
}

//------------------------------------------------------------------------
void VisualEffects::startEffect(const Effect& effect) {
    currentEffect_ = effect;
    isPlaying_ = true;
    startTime_ = std::chrono::steady_clock::now();
}

//------------------------------------------------------------------------
void VisualEffects::stopEffect() {
    isPlaying_ = false;
}

//------------------------------------------------------------------------
int VisualEffects::getElapsedMs() const {
    if (!isPlaying_) return 0;
    auto now = std::chrono::steady_clock::now();
    return static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime_).count()
    );
}

//------------------------------------------------------------------------
bool VisualEffects::update(FlaschenTaschenClient& client) {
    if (!isPlaying_) return false;

    int elapsed = getElapsedMs();

    // Check if effect has finished
    if (elapsed >= currentEffect_.durationMs) {
        isPlaying_ = false;
        return false;
    }

    // Calculate normalized time (0.0 - 1.0)
    float t = static_cast<float>(elapsed) / currentEffect_.durationMs;

    // Render based on effect type
    switch (currentEffect_.type) {
        case EffectType::SolidColor:
            renderSolidColor(client,
                currentEffect_.color1R,
                currentEffect_.color1G,
                currentEffect_.color1B);
            break;

        case EffectType::ColorRamp:
            renderColorRamp(client,
                currentEffect_.color1R, currentEffect_.color1G, currentEffect_.color1B,
                currentEffect_.color2R, currentEffect_.color2G, currentEffect_.color2B,
                currentEffect_.rampDirection);
            break;

        case EffectType::Pulse:
            renderPulse(client, t);
            break;

        case EffectType::Rainbow:
            renderAnimatedRainbow(client, t);
            break;

        case EffectType::Flash:
            renderFlash(client, t);
            break;

        case EffectType::Strobe:
            renderStrobe(client, t);
            break;

        case EffectType::Wave:
            renderWave(client, t);
            break;

        case EffectType::Sparkle:
            renderSparkle(client, t);
            break;

        default:
            break;
    }

    return true;
}

//------------------------------------------------------------------------
void VisualEffects::renderSolidColor(FlaschenTaschenClient& client,
                                      uint8_t r, uint8_t g, uint8_t b) {
    client.clear(Color(r, g, b));
}

//------------------------------------------------------------------------
void VisualEffects::renderColorRamp(FlaschenTaschenClient& client,
                                     uint8_t r1, uint8_t g1, uint8_t b1,
                                     uint8_t r2, uint8_t g2, uint8_t b2,
                                     RampDirection direction) {
    int width = client.getWidth();
    int height = client.getHeight();
    Color c1(r1, g1, b1);
    Color c2(r2, g2, b2);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float t = 0.0f;

            switch (direction) {
                case RampDirection::Horizontal:
                    t = static_cast<float>(x) / (width - 1);
                    break;

                case RampDirection::Vertical:
                    t = static_cast<float>(y) / (height - 1);
                    break;

                case RampDirection::DiagonalDown:
                    t = static_cast<float>(x + y) / (width + height - 2);
                    break;

                case RampDirection::DiagonalUp:
                    t = static_cast<float>(x + (height - 1 - y)) / (width + height - 2);
                    break;

                case RampDirection::Radial: {
                    float cx = width / 2.0f;
                    float cy = height / 2.0f;
                    float maxDist = std::sqrt(cx * cx + cy * cy);
                    float dx = x - cx;
                    float dy = y - cy;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    t = dist / maxDist;
                    break;
                }
            }

            Color c = lerpColor(c1, c2, t);
            client.setPixel(x, y, c);
        }
    }
}

//------------------------------------------------------------------------
void VisualEffects::renderRainbow(FlaschenTaschenClient& client, float phase) {
    int width = client.getWidth();
    int height = client.getHeight();

    for (int x = 0; x < width; ++x) {
        float hue = std::fmod(phase + static_cast<float>(x) / width, 1.0f);
        Color c = hsvToRgb(hue, 1.0f, 1.0f);

        for (int y = 0; y < height; ++y) {
            client.setPixel(x, y, c);
        }
    }
}

//------------------------------------------------------------------------
void VisualEffects::renderPulse(FlaschenTaschenClient& client, float t) {
    // Pulse uses sine wave for smooth fade in/out
    int elapsed = getElapsedMs();
    float periodSec = currentEffect_.periodMs / 1000.0f;
    float phase = std::fmod(elapsed / 1000.0f, periodSec) / periodSec;
    float brightness = 0.5f + 0.5f * std::sin(phase * 2.0f * static_cast<float>(M_PI));

    brightness *= currentEffect_.intensity;

    uint8_t r = static_cast<uint8_t>(currentEffect_.color1R * brightness);
    uint8_t g = static_cast<uint8_t>(currentEffect_.color1G * brightness);
    uint8_t b = static_cast<uint8_t>(currentEffect_.color1B * brightness);

    client.clear(Color(r, g, b));
}

//------------------------------------------------------------------------
void VisualEffects::renderFlash(FlaschenTaschenClient& client, float t) {
    // Quick flash that fades out
    float brightness = (1.0f - t) * currentEffect_.intensity;
    brightness = std::max(0.0f, brightness);

    uint8_t r = static_cast<uint8_t>(currentEffect_.color1R * brightness);
    uint8_t g = static_cast<uint8_t>(currentEffect_.color1G * brightness);
    uint8_t b = static_cast<uint8_t>(currentEffect_.color1B * brightness);

    client.clear(Color(r, g, b));
}

//------------------------------------------------------------------------
void VisualEffects::renderStrobe(FlaschenTaschenClient& client, float t) {
    int elapsed = getElapsedMs();
    int periodMs = currentEffect_.periodMs;
    bool on = ((elapsed / periodMs) % 2) == 0;

    if (on) {
        client.clear(Color(
            currentEffect_.color1R,
            currentEffect_.color1G,
            currentEffect_.color1B
        ));
    } else {
        client.clear(Color(
            currentEffect_.color2R,
            currentEffect_.color2G,
            currentEffect_.color2B
        ));
    }
}

//------------------------------------------------------------------------
void VisualEffects::renderWave(FlaschenTaschenClient& client, float t) {
    int width = client.getWidth();
    int height = client.getHeight();
    int elapsed = getElapsedMs();

    float speed = currentEffect_.speed / 50.0f;  // Normalize speed
    float wavePhase = elapsed * speed * 0.01f;

    Color c1(currentEffect_.color1R, currentEffect_.color1G, currentEffect_.color1B);
    Color c2(currentEffect_.color2R, currentEffect_.color2G, currentEffect_.color2B);

    for (int x = 0; x < width; ++x) {
        float xPhase = static_cast<float>(x) / width * 2.0f * static_cast<float>(M_PI);
        float wave = 0.5f + 0.5f * std::sin(xPhase + wavePhase);

        Color c = lerpColor(c1, c2, wave);

        for (int y = 0; y < height; ++y) {
            client.setPixel(x, y, c);
        }
    }
}

//------------------------------------------------------------------------
void VisualEffects::renderSparkle(FlaschenTaschenClient& client, float t) {
    int width = client.getWidth();
    int height = client.getHeight();

    // Clear with background color
    client.clear(Color(
        currentEffect_.color2R,
        currentEffect_.color2G,
        currentEffect_.color2B
    ));

    // Add random sparkles
    int numSparkles = static_cast<int>((width * height) * 0.1f * currentEffect_.intensity);
    std::uniform_int_distribution<int> xDist(0, width - 1);
    std::uniform_int_distribution<int> yDist(0, height - 1);
    std::uniform_real_distribution<float> brightDist(0.5f, 1.0f);

    for (int i = 0; i < numSparkles; ++i) {
        int x = xDist(rng_);
        int y = yDist(rng_);
        float bright = brightDist(rng_);

        uint8_t r = static_cast<uint8_t>(currentEffect_.color1R * bright);
        uint8_t g = static_cast<uint8_t>(currentEffect_.color1G * bright);
        uint8_t b = static_cast<uint8_t>(currentEffect_.color1B * bright);

        client.setPixel(x, y, Color(r, g, b));
    }
}

//------------------------------------------------------------------------
void VisualEffects::renderAnimatedRainbow(FlaschenTaschenClient& client, float t) {
    float speed = currentEffect_.speed / 50.0f;
    int elapsed = getElapsedMs();
    float phase = std::fmod(elapsed * speed * 0.001f, 1.0f);
    renderRainbow(client, phase);
}

//------------------------------------------------------------------------
Color VisualEffects::lerpColor(const Color& c1, const Color& c2, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    return Color(
        static_cast<uint8_t>(c1.r + (c2.r - c1.r) * t),
        static_cast<uint8_t>(c1.g + (c2.g - c1.g) * t),
        static_cast<uint8_t>(c1.b + (c2.b - c1.b) * t)
    );
}

//------------------------------------------------------------------------
Color VisualEffects::hsvToRgb(float h, float s, float v) {
    // h, s, v in range [0, 1]
    float r = 0, g = 0, b = 0;

    int i = static_cast<int>(h * 6.0f);
    float f = h * 6.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);

    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
    }

    return Color(
        static_cast<uint8_t>(r * 255),
        static_cast<uint8_t>(g * 255),
        static_cast<uint8_t>(b * 255)
    );
}

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
