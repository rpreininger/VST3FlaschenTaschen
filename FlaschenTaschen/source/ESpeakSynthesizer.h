//------------------------------------------------------------------------
// Copyright(c) 2024 Stratojets.
//------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#endif

namespace FlaschenTaschen {

//------------------------------------------------------------------------
// ESpeakSynthesizer - Text-to-speech using eSpeak-NG library
// Uses dynamic loading on Windows to avoid link-time dependency
//------------------------------------------------------------------------
class ESpeakSynthesizer {
public:
    ESpeakSynthesizer();
    ~ESpeakSynthesizer();

    // Initialize the synthesizer (must be called before use)
    bool initialize(int sampleRate = 44100);

    // Shutdown the synthesizer
    void shutdown();

    // Check if initialized
    bool isInitialized() const { return initialized_; }

    // Set voice parameters
    void setVoice(const std::string& voice);  // e.g., "en", "de", "fr"
    void setRate(int rate);     // Words per minute (80-450, default 175)
    void setPitch(int pitch);   // Pitch (0-99, default 50)
    void setVolume(int volume); // Volume (0-200, default 100)

    // Speak text (generates audio samples)
    void speak(const std::string& text);

    // Stop current speech
    void stop();

    // Check if currently speaking
    bool isSpeaking() const { return speaking_; }

    // Get generated audio samples (and clear buffer)
    std::vector<float> getAudioSamples();

    // Get number of available samples
    size_t getAvailableSamples() const;

    // Read samples into buffer without clearing
    size_t readSamples(float* buffer, size_t maxSamples);

    // Get sample rate
    int getSampleRate() const { return sampleRate_; }

    // Get last error message
    const std::string& getLastError() const { return lastError_; }

private:
    bool initialized_ = false;
    bool dllLoaded_ = false;
    int sampleRate_ = 44100;
    std::atomic<bool> speaking_{false};
    std::string lastError_;

    // Audio buffer
    std::vector<float> audioBuffer_;
    mutable std::mutex bufferMutex_;

    // Voice settings
    std::string voice_ = "en";
    int rate_ = 175;
    int pitch_ = 50;
    int volume_ = 100;

#ifdef _WIN32
    HMODULE espeakDll_ = nullptr;
#endif

    // Function pointers for dynamic loading
    void* fn_Initialize_ = nullptr;
    void* fn_SetSynthCallback_ = nullptr;
    void* fn_SetParameter_ = nullptr;
    void* fn_SetVoiceByName_ = nullptr;
    void* fn_Synth_ = nullptr;
    void* fn_Synchronize_ = nullptr;
    void* fn_Cancel_ = nullptr;
    void* fn_Terminate_ = nullptr;

    bool loadDll();
    void unloadDll();
    void appendSamples(const short* samples, int count);

    // Static callback handling
    static ESpeakSynthesizer* currentInstance_;
    static std::mutex instanceMutex_;
    static int synthCallbackStatic(short* wav, int numsamples, void* events);
};

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
