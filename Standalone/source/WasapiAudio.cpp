//------------------------------------------------------------------------
// FlaschenTaschen Standalone - WASAPI Audio Output
//------------------------------------------------------------------------

#include "WasapiAudio.h"
#include <comdef.h>
#include <cstring>
#include <string>

// Link required libraries
#pragma comment(lib, "ole32.lib")

namespace FlaschenTaschen {

//------------------------------------------------------------------------
WasapiAudio::WasapiAudio() {
}

//------------------------------------------------------------------------
WasapiAudio::~WasapiAudio() {
    stop();

    if (waveFormat_) {
        CoTaskMemFree(waveFormat_);
        waveFormat_ = nullptr;
    }
    if (renderClient_) {
        renderClient_->Release();
        renderClient_ = nullptr;
    }
    if (audioClient_) {
        audioClient_->Release();
        audioClient_ = nullptr;
    }
    if (device_) {
        device_->Release();
        device_ = nullptr;
    }
    if (deviceEnumerator_) {
        deviceEnumerator_->Release();
        deviceEnumerator_ = nullptr;
    }
    if (audioEvent_) {
        CloseHandle(audioEvent_);
        audioEvent_ = nullptr;
    }
}

//------------------------------------------------------------------------
bool WasapiAudio::initialize() {
    HRESULT hr;

    // Initialize COM
    hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        lastError_ = "Failed to initialize COM";
        return false;
    }

    // Create device enumerator
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&deviceEnumerator_
    );
    if (FAILED(hr)) {
        lastError_ = "Failed to create device enumerator";
        return false;
    }

    // Get default audio endpoint
    hr = deviceEnumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &device_);
    if (FAILED(hr)) {
        lastError_ = "Failed to get default audio device";
        return false;
    }

    // Activate audio client
    hr = device_->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL,
        nullptr,
        (void**)&audioClient_
    );
    if (FAILED(hr)) {
        lastError_ = "Failed to activate audio client";
        return false;
    }

    // Get mix format (shared mode format)
    hr = audioClient_->GetMixFormat(&waveFormat_);
    if (FAILED(hr)) {
        lastError_ = "Failed to get mix format";
        return false;
    }

    // Store format info
    sampleRate_ = waveFormat_->nSamplesPerSec;
    numChannels_ = waveFormat_->nChannels;

    // Request 20ms buffer
    REFERENCE_TIME requestedDuration = 200000; // 20ms in 100ns units

    // Initialize audio client in shared mode (more compatible than exclusive)
    hr = audioClient_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        requestedDuration,
        0,
        waveFormat_,
        nullptr
    );
    if (FAILED(hr)) {
        lastError_ = "Failed to initialize audio client (hr=" + std::to_string(hr) + ")";
        return false;
    }

    // Get actual buffer size
    UINT32 bufferFrameCount;
    hr = audioClient_->GetBufferSize(&bufferFrameCount);
    if (FAILED(hr)) {
        lastError_ = "Failed to get buffer size";
        return false;
    }
    bufferFrames_ = bufferFrameCount;

    // Create event for audio callback
    audioEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!audioEvent_) {
        lastError_ = "Failed to create audio event";
        return false;
    }

    hr = audioClient_->SetEventHandle(audioEvent_);
    if (FAILED(hr)) {
        lastError_ = "Failed to set event handle";
        return false;
    }

    // Get render client
    hr = audioClient_->GetService(
        __uuidof(IAudioRenderClient),
        (void**)&renderClient_
    );
    if (FAILED(hr)) {
        lastError_ = "Failed to get render client";
        return false;
    }

    return true;
}

//------------------------------------------------------------------------
bool WasapiAudio::start(AudioCallback callback) {
    if (running_) {
        return true;
    }

    if (!audioClient_ || !renderClient_) {
        lastError_ = "Audio not initialized";
        return false;
    }

    callback_ = callback;
    running_ = true;

    // Start audio thread
    audioThread_ = std::thread(&WasapiAudio::audioThread, this);

    // Start the audio client
    HRESULT hr = audioClient_->Start();
    if (FAILED(hr)) {
        running_ = false;
        if (audioThread_.joinable()) {
            audioThread_.join();
        }
        lastError_ = "Failed to start audio client";
        return false;
    }

    return true;
}

//------------------------------------------------------------------------
void WasapiAudio::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // Signal the event to wake up the thread
    if (audioEvent_) {
        SetEvent(audioEvent_);
    }

    if (audioThread_.joinable()) {
        audioThread_.join();
    }

    if (audioClient_) {
        audioClient_->Stop();
        audioClient_->Reset();
    }
}

//------------------------------------------------------------------------
void WasapiAudio::audioThread() {
    // Set thread priority for audio
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    while (running_) {
        // Wait for buffer event
        DWORD result = WaitForSingleObject(audioEvent_, 100);

        if (!running_) break;

        if (result != WAIT_OBJECT_0) {
            continue;
        }

        // Get padding (how much data is already in buffer)
        UINT32 padding = 0;
        HRESULT hr = audioClient_->GetCurrentPadding(&padding);
        if (FAILED(hr)) {
            continue;
        }

        // Calculate available frames
        UINT32 availableFrames = bufferFrames_ - padding;
        if (availableFrames == 0) {
            continue;
        }

        // Get buffer from WASAPI
        BYTE* buffer = nullptr;
        hr = renderClient_->GetBuffer(availableFrames, &buffer);
        if (FAILED(hr)) {
            continue;
        }

        // Check if format is float
        bool isFloat = (waveFormat_->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
                       (waveFormat_->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                        reinterpret_cast<WAVEFORMATEXTENSIBLE*>(waveFormat_)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);

        if (isFloat && callback_) {
            // Direct float output
            callback_(reinterpret_cast<float*>(buffer), availableFrames, numChannels_);
        }
        else if (callback_) {
            // Need to convert - create temp float buffer
            std::vector<float> tempBuffer(availableFrames * numChannels_);
            callback_(tempBuffer.data(), availableFrames, numChannels_);

            // Convert float to 16-bit PCM
            int16_t* pcmBuffer = reinterpret_cast<int16_t*>(buffer);
            for (UINT32 i = 0; i < availableFrames * numChannels_; ++i) {
                float sample = tempBuffer[i];
                sample = (sample < -1.0f) ? -1.0f : (sample > 1.0f) ? 1.0f : sample;
                pcmBuffer[i] = static_cast<int16_t>(sample * 32767.0f);
            }
        }
        else {
            // No callback - silence
            memset(buffer, 0, availableFrames * waveFormat_->nBlockAlign);
        }

        // Release buffer
        renderClient_->ReleaseBuffer(availableFrames, 0);
    }
}

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
