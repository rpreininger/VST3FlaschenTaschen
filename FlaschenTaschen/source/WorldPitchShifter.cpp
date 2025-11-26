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
    // Analyze input to find average F0, then calculate ratio
    // For simplicity, assume base frequency and calculate ratio
    double baseFreq = 150.0;  // Will be refined during analysis
    double ratio = targetFreqHz / baseFreq;
    return pitchShiftWorld(input, ratio);
}

//------------------------------------------------------------------------
std::vector<float> WorldPitchShifter::pitchShiftWorld(const std::vector<float>& input, double ratio) {
    if (input.empty() || ratio <= 0) {
        return input;
    }

    int x_length = static_cast<int>(input.size());

    // Convert float to double for World
    std::vector<double> x(x_length);
    for (int i = 0; i < x_length; ++i) {
        x[i] = static_cast<double>(input[i]);
    }

    // Initialize DIO (F0 estimation)
    DioOption dio_option;
    InitializeDioOption(&dio_option);
    dio_option.frame_period = framePeriod_;
    dio_option.speed = 1;
    dio_option.f0_floor = 50.0;
    dio_option.f0_ceil = 800.0;
    dio_option.allowed_range = 0.1;

    int f0_length = GetSamplesForDIO(sampleRate_, x_length, dio_option.frame_period);

    std::vector<double> f0(f0_length);
    std::vector<double> temporal_positions(f0_length);

    // Extract F0
    Dio(x.data(), x_length, sampleRate_, &dio_option,
        temporal_positions.data(), f0.data());

    // Initialize CheapTrick (spectral envelope)
    CheapTrickOption cheaptrick_option;
    InitializeCheapTrickOption(sampleRate_, &cheaptrick_option);
    cheaptrick_option.f0_floor = dio_option.f0_floor;

    int fft_size = GetFFTSizeForCheapTrick(sampleRate_, &cheaptrick_option);

    // Allocate spectrogram
    std::vector<double*> spectrogram(f0_length);
    for (int i = 0; i < f0_length; ++i) {
        spectrogram[i] = new double[fft_size / 2 + 1];
    }

    // Extract spectral envelope
    CheapTrick(x.data(), x_length, sampleRate_,
               temporal_positions.data(), f0.data(), f0_length,
               &cheaptrick_option, spectrogram.data());

    // Initialize D4C (aperiodicity)
    D4COption d4c_option;
    InitializeD4COption(&d4c_option);

    // Allocate aperiodicity
    std::vector<double*> aperiodicity(f0_length);
    for (int i = 0; i < f0_length; ++i) {
        aperiodicity[i] = new double[fft_size / 2 + 1];
    }

    // Extract aperiodicity
    D4C(x.data(), x_length, sampleRate_,
        temporal_positions.data(), f0.data(), f0_length,
        fft_size, &d4c_option, aperiodicity.data());

    // Modify F0 (pitch shift)
    std::vector<double> f0_modified(f0_length);
    for (int i = 0; i < f0_length; ++i) {
        if (f0[i] > 0) {
            f0_modified[i] = f0[i] * ratio;
            // Clamp to reasonable range
            f0_modified[i] = (std::max)(50.0, (std::min)(800.0, f0_modified[i]));
        } else {
            f0_modified[i] = 0;  // Unvoiced
        }
    }

    // Synthesize with modified F0
    int y_length = static_cast<int>((f0_length - 1) * framePeriod_ / 1000.0 * sampleRate_) + 1;
    std::vector<double> y(y_length, 0.0);

    Synthesis(f0_modified.data(), f0_length,
              spectrogram.data(), aperiodicity.data(),
              fft_size, framePeriod_, sampleRate_, y_length, y.data());

    // Convert back to float
    std::vector<float> output(y_length);
    for (int i = 0; i < y_length; ++i) {
        output[i] = static_cast<float>(y[i]);
    }

    // Cleanup
    for (int i = 0; i < f0_length; ++i) {
        delete[] spectrogram[i];
        delete[] aperiodicity[i];
    }

    return output;
}

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
