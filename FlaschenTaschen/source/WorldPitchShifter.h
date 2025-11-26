//------------------------------------------------------------------------
// Copyright(c) 2024 Stratojets.
//------------------------------------------------------------------------

#pragma once

#include <vector>
#include <cstdint>

namespace FlaschenTaschen {

//------------------------------------------------------------------------
// WorldPitchShifter - Pitch shifting using World vocoder
// Analyzes audio, modifies F0 (pitch), and resynthesizes
//------------------------------------------------------------------------
class WorldPitchShifter {
public:
    WorldPitchShifter();
    ~WorldPitchShifter();

    // Initialize with sample rate
    void initialize(int sampleRate);

    // Set target pitch as MIDI note (60 = C4 = 261.63 Hz)
    void setTargetMidiNote(int midiNote);

    // Set pitch shift in semitones (0 = no shift, +12 = octave up)
    void setPitchShiftSemitones(double semitones);

    // Set pitch shift ratio (1.0 = no change, 2.0 = octave up)
    void setPitchShiftRatio(double ratio);

    // Process audio samples - pitch shift in place
    // Input: mono float samples from TTS
    // Output: pitch-shifted samples
    std::vector<float> process(const std::vector<float>& input);

    // Process with specific target frequency
    std::vector<float> processToFrequency(const std::vector<float>& input, double targetFreqHz);

    // Get current settings
    int getSampleRate() const { return sampleRate_; }
    double getPitchShiftRatio() const { return pitchShiftRatio_; }

    // Convert MIDI note to frequency
    static double midiNoteToFrequency(int midiNote);

    // Convert frequency to MIDI note
    static int frequencyToMidiNote(double frequency);

private:
    int sampleRate_ = 44100;
    double pitchShiftRatio_ = 1.0;
    double framePeriod_ = 5.0;  // ms

    // Internal processing
    std::vector<float> pitchShiftWorld(const std::vector<float>& input, double ratio);
};

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
