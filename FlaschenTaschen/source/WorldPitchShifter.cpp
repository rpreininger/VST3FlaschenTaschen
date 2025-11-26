//------------------------------------------------------------------------
// Copyright(c) 2024 Stratojets.
//------------------------------------------------------------------------

#include "WorldPitchShifter.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>

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

    // Limit shift to +/- 36 semitones (3 octaves) to support wide range
    // C2 (65 Hz) to C7 (2093 Hz) relative to C4 (262 Hz)
    semitones = (std::max)(-36.0, (std::min)(36.0, semitones));

    double ratio = std::pow(2.0, semitones / 12.0);
    return pitchShiftWorld(input, ratio);
}

//------------------------------------------------------------------------
std::vector<float> WorldPitchShifter::pitchShiftWorld(const std::vector<float>& input, double ratio) {
    if (input.empty() || ratio <= 0 || std::abs(ratio - 1.0) < 0.001) {
        return input;
    }

    int inputLength = static_cast<int>(input.size());

    // Convert float input to double for World vocoder
    std::vector<double> x(inputLength);
    for (int i = 0; i < inputLength; ++i) {
        x[i] = static_cast<double>(input[i]);
    }

    // Step 1: F0 extraction with DIO
    DioOption dioOption;
    InitializeDioOption(&dioOption);
    dioOption.frame_period = framePeriod_;
    dioOption.speed = 1;  // Highest quality
    dioOption.f0_floor = 71.0;   // Lower limit for F0
    dioOption.f0_ceil = 800.0;   // Upper limit for F0
    dioOption.allowed_range = 0.1;

    int f0Length = GetSamplesForDIO(sampleRate_, inputLength, framePeriod_);

    std::vector<double> f0(f0Length);
    std::vector<double> temporalPositions(f0Length);

    Dio(x.data(), inputLength, sampleRate_, &dioOption,
        temporalPositions.data(), f0.data());

    // Step 2: Spectral envelope with CheapTrick
    CheapTrickOption cheapTrickOption;
    InitializeCheapTrickOption(sampleRate_, &cheapTrickOption);
    int fftSize = GetFFTSizeForCheapTrick(sampleRate_, &cheapTrickOption);

    // Allocate spectrogram (f0Length x (fftSize/2 + 1))
    int specLength = fftSize / 2 + 1;
    double** spectrogram = new double*[f0Length];
    for (int i = 0; i < f0Length; ++i) {
        spectrogram[i] = new double[specLength];
    }

    CheapTrick(x.data(), inputLength, sampleRate_,
               temporalPositions.data(), f0.data(), f0Length,
               &cheapTrickOption, spectrogram);

    // Step 3: Aperiodicity with D4C
    D4COption d4cOption;
    InitializeD4COption(&d4cOption);

    double** aperiodicity = new double*[f0Length];
    for (int i = 0; i < f0Length; ++i) {
        aperiodicity[i] = new double[specLength];
    }

    D4C(x.data(), inputLength, sampleRate_,
        temporalPositions.data(), f0.data(), f0Length,
        fftSize, &d4cOption, aperiodicity);

    // Step 4: Modify F0 for pitch shift (duration stays the same!)
    std::vector<double> modifiedF0(f0Length);
    for (int i = 0; i < f0Length; ++i) {
        if (f0[i] > 0) {
            // Scale F0 by pitch ratio - this changes pitch without affecting duration
            modifiedF0[i] = f0[i] * ratio;
        } else {
            // Unvoiced frame - keep as 0
            modifiedF0[i] = 0.0;
        }
    }

    // Step 5: Synthesis with modified F0 but same frame count = same duration
    int outputLength = inputLength;  // Same length as input!
    std::vector<double> y(outputLength);

    Synthesis(modifiedF0.data(), f0Length,
              spectrogram, aperiodicity,
              fftSize, framePeriod_, sampleRate_,
              outputLength, y.data());

    // Convert back to float
    std::vector<float> output(outputLength);
    for (int i = 0; i < outputLength; ++i) {
        output[i] = static_cast<float>(y[i]);
    }

    // Cleanup
    for (int i = 0; i < f0Length; ++i) {
        delete[] spectrogram[i];
        delete[] aperiodicity[i];
    }
    delete[] spectrogram;
    delete[] aperiodicity;

    return output;
}

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
