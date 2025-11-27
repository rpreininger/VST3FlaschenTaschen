//------------------------------------------------------------------------
// FlaschenTaschen Standalone - WASAPI Audio Output
//------------------------------------------------------------------------

#include "WasapiAudio.h"
#include <comdef.h>
#include <cstring>
#include <string>
#include <locale>
#include <codecvt>

// Link required libraries
#pragma comment(lib, "ole32.lib")

namespace FlaschenTaschen {

// Helper to convert wide string to UTF-8
static std::string wideToUtf8(const wchar_t* wide) {
    if (!wide) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string result(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, &result[0], len, nullptr, nullptr);
    return result;
}

// Helper to convert UTF-8 to wide string
static std::wstring utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring result(len - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &result[0], len);
    return result;
}

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
std::vector<AudioDeviceInfo> WasapiAudio::enumerateDevices() {
    std::vector<AudioDeviceInfo> devices;
    HRESULT hr;

    // Initialize COM
    hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return devices;
    }

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&enumerator
    );
    if (FAILED(hr) || !enumerator) {
        return devices;
    }

    // Get default device ID for comparison
    std::wstring defaultDeviceId;
    IMMDevice* defaultDevice = nullptr;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice))) {
        LPWSTR deviceId = nullptr;
        if (SUCCEEDED(defaultDevice->GetId(&deviceId))) {
            defaultDeviceId = deviceId;
            CoTaskMemFree(deviceId);
        }
        defaultDevice->Release();
    }

    // Enumerate all render devices
    IMMDeviceCollection* collection = nullptr;
    hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
    if (SUCCEEDED(hr) && collection) {
        UINT count = 0;
        collection->GetCount(&count);

        for (UINT i = 0; i < count; ++i) {
            IMMDevice* device = nullptr;
            if (SUCCEEDED(collection->Item(i, &device))) {
                AudioDeviceInfo info;

                // Get device ID
                LPWSTR deviceId = nullptr;
                if (SUCCEEDED(device->GetId(&deviceId))) {
                    info.id = wideToUtf8(deviceId);
                    info.isDefault = (deviceId == defaultDeviceId);
                    CoTaskMemFree(deviceId);
                }

                // Get friendly name
                IPropertyStore* props = nullptr;
                if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props))) {
                    PROPVARIANT varName;
                    PropVariantInit(&varName);
                    if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName))) {
                        info.name = wideToUtf8(varName.pwszVal);
                        PropVariantClear(&varName);
                    }
                    props->Release();
                }

                devices.push_back(info);
                device->Release();
            }
        }
        collection->Release();
    }

    enumerator->Release();
    return devices;
}

//------------------------------------------------------------------------
bool WasapiAudio::initialize() {
    return initialize("", 0);  // Use default device and buffer
}

//------------------------------------------------------------------------
bool WasapiAudio::initialize(const std::string& deviceId, int bufferMs) {
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

    // Get audio endpoint - either specific device or default
    if (deviceId.empty()) {
        hr = deviceEnumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &device_);
        if (FAILED(hr)) {
            lastError_ = "Failed to get default audio device";
            return false;
        }
    } else {
        std::wstring wideId = utf8ToWide(deviceId);
        hr = deviceEnumerator_->GetDevice(wideId.c_str(), &device_);
        if (FAILED(hr)) {
            lastError_ = "Failed to get audio device: " + deviceId;
            return false;
        }
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

    // Calculate buffer duration (in 100ns units)
    // Default to 20ms if not specified or invalid
    if (bufferMs <= 0) bufferMs = 20;
    if (bufferMs > 500) bufferMs = 500;  // Cap at 500ms
    REFERENCE_TIME requestedDuration = bufferMs * 10000; // ms to 100ns units

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
