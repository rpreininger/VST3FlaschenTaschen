//------------------------------------------------------------------------
// FlaschenTaschen Standalone - WASAPI Audio Output
//------------------------------------------------------------------------

#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>

namespace FlaschenTaschen {

//------------------------------------------------------------------------
// WasapiAudio - Simple WASAPI exclusive mode audio output
//------------------------------------------------------------------------
class WasapiAudio {
public:
    // Audio callback: fills buffer with samples, returns number of frames written
    using AudioCallback = std::function<void(float* buffer, int numFrames, int numChannels)>;

    WasapiAudio();
    ~WasapiAudio();

    // Initialize WASAPI (finds default output device)
    bool initialize();

    // Start audio playback with callback
    bool start(AudioCallback callback);

    // Stop audio playback
    void stop();

    // Check if running
    bool isRunning() const { return running_; }

    // Get format info
    int getSampleRate() const { return sampleRate_; }
    int getNumChannels() const { return numChannels_; }
    int getBufferFrames() const { return bufferFrames_; }

    // Get last error
    const std::string& getLastError() const { return lastError_; }

private:
    void audioThread();

    IMMDeviceEnumerator* deviceEnumerator_ = nullptr;
    IMMDevice* device_ = nullptr;
    IAudioClient* audioClient_ = nullptr;
    IAudioRenderClient* renderClient_ = nullptr;

    WAVEFORMATEX* waveFormat_ = nullptr;
    int sampleRate_ = 44100;
    int numChannels_ = 2;
    int bufferFrames_ = 0;

    std::atomic<bool> running_{false};
    std::thread audioThread_;
    AudioCallback callback_;
    std::string lastError_;

    HANDLE audioEvent_ = nullptr;
};

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
