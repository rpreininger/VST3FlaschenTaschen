//------------------------------------------------------------------------
// Copyright(c) 2024 Stratojets.
//------------------------------------------------------------------------

#include "WorldPitchShifter.h"
#include <cmath>
#include <algorithm>

// World vocoder headers
#include "world/dio.h"
#include "world/cheaptrick.h"
#include "world/d4c.h"
#include "world/synthesis.h"

namespace FlaschenTaschen {

//------------------------------------------------------------------------
WorldPitchShifter::WorldPitchShifter() {
}

//------------------------------------------------------------------------
WorldPitchShifter::~WorldPitchShifter() {
}

//------------------------------------------------------------------------
void WorldPitchShifter::initialize(int sampleRate) {
    sampleRate_ = sampleRate;
}

//------------------------------------------------------------------------
double WorldPitchShifter::midiNoteToFrequency(int midiNote) {
    // A4 (MIDI 69) = 440 Hz
    return 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
}

//------------------------------------------------------------------------
int WorldPitchShifter::frequencyToMidiNote(double frequency) {
    if (frequency <= 0) return 0;
    return static_cast<int>(std::round(69.0 + 12.0 * std::log2(frequency / 440.0)));
}

//------------------------------------------------------------------------
void WorldPitchShifter::setTargetMidiNote(int midiNote) {
    // We'll shift relative to a base frequency
    // Assume input is around 150 Hz (typical speech)
    double targetFreq = midiNoteToFrequency(midiNote);
    double baseFreq = 150.0;  // Approximate speech F0
    pitchShiftRatio_ = targetFreq / baseFreq;
}

//------------------------------------------------------------------------
void WorldPitchShifter::setPitchShiftSemitones(double semitones) {
    pitchShiftRatio_ = std::pow(2.0, semitones / 12.0);
}

//------------------------------------------------------------------------
void WorldPitchShifter::setPitchShiftRatio(double ratio) {
    pitchShiftRatio_ = ratio;
}

//------------------------------------------------------------------------
std::vector<float> WorldPitchShifter::process(const std::vector<float>& input) {
    return pitchShiftWorld(input, pitchShiftRatio_);
}

//------------------------------------------------------------------------
std::vector<float> WorldPitchShifter::processToFrequency(const std::vector<float>& input, double targetFreqHz) {
    // Calculate semitone shift from middle C (261.63 Hz, MIDI 60)
    // This gives a reasonable musical shift range
    double middleC = 261.63;
    double semitones = 12.0 * std::log2(targetFreqHz / middleC);

    // Limit shift to +/- 12 semitones (1 octave) to preserve voice quality
    semitones = (std::max)(-12.0, (std::min)(12.0, semitones));

    double ratio = std::pow(2.0, semitones / 12.0);
    return pitchShiftWorld(input, ratio);
}

//------------------------------------------------------------------------
std::vector<float> WorldPitchShifter::pitchShiftWorld(const std::vector<float>& input, double ratio) {
    if (input.empty() || ratio <= 0 || std::abs(ratio - 1.0) < 0.001) {
        return input;
    }

    // Simple resampling-based pitch shift
    // ratio > 1 = higher pitch (faster playback resampled back)
    // ratio < 1 = lower pitch (slower playback resampled back)

    // To raise pitch by ratio, we read input faster (skip samples)
    // To lower pitch by ratio, we read input slower (interpolate more)

    int inputLength = static_cast<int>(input.size());
    int outputLength = inputLength;  // Keep same duration

    std::vector<float> output(outputLength);

    for (int i = 0; i < outputLength; ++i) {
        // Read from input at ratio speed
        double srcPos = i * ratio;
        int srcIndex = static_cast<int>(srcPos);
        double frac = srcPos - srcIndex;

        // Wrap around or clamp for continuous sound
        srcIndex = srcIndex % inputLength;
        int nextIndex = (srcIndex + 1) % inputLength;

        // Linear interpolation
        output[i] = static_cast<float>(input[srcIndex] * (1.0 - frac) + input[nextIndex] * frac);
    }

    return output;
}

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
