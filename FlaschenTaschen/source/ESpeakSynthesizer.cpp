//------------------------------------------------------------------------
// Copyright(c) 2024 Stratojets.
//------------------------------------------------------------------------

#include "ESpeakSynthesizer.h"
#include <cstring>
#include <algorithm>

namespace FlaschenTaschen {

// Static members
ESpeakSynthesizer* ESpeakSynthesizer::currentInstance_ = nullptr;
std::mutex ESpeakSynthesizer::instanceMutex_;

// eSpeak constants (from speak_lib.h)
namespace {
    constexpr int AUDIO_OUTPUT_SYNCHRONOUS = 0x02;
    constexpr int espeakCHARS_AUTO = 0;
    constexpr int espeakENDPAUSE = 0x1000;
    constexpr int espeakRATE = 1;
    constexpr int espeakVOLUME = 2;
    constexpr int espeakPITCH = 3;
    constexpr int espeakRANGE = 4;  // Pitch range/variation
    constexpr int POS_CHARACTER = 1;
}

// Function pointer types
typedef int (*espeak_Initialize_t)(int, int, const char*, int);
typedef void (*espeak_SetSynthCallback_t)(void*);
typedef int (*espeak_SetParameter_t)(int, int, int);
typedef int (*espeak_SetVoiceByName_t)(const char*);
typedef int (*espeak_Synth_t)(const void*, size_t, unsigned int, int, unsigned int, unsigned int, unsigned int*, void*);
typedef int (*espeak_Synchronize_t)(void);
typedef int (*espeak_Cancel_t)(void);
typedef int (*espeak_Terminate_t)(void);

//------------------------------------------------------------------------
ESpeakSynthesizer::ESpeakSynthesizer() {
}

//------------------------------------------------------------------------
ESpeakSynthesizer::~ESpeakSynthesizer() {
    shutdown();
    unloadDll();
}

//------------------------------------------------------------------------
bool ESpeakSynthesizer::loadDll() {
#ifdef _WIN32
    if (dllLoaded_) return true;

    // Try multiple paths
    const char* dllPaths[] = {
        "C:\\Program Files\\eSpeak NG\\libespeak-ng.dll",
        "C:\\Program Files (x86)\\eSpeak NG\\libespeak-ng.dll",
        "libespeak-ng.dll",
        nullptr
    };

    for (int i = 0; dllPaths[i] != nullptr; ++i) {
        espeakDll_ = LoadLibraryA(dllPaths[i]);
        if (espeakDll_) break;
    }

    if (!espeakDll_) {
        lastError_ = "Failed to load libespeak-ng.dll";
        return false;
    }

    // Load function pointers
    fn_Initialize_ = (void*)GetProcAddress(espeakDll_, "espeak_Initialize");
    fn_SetSynthCallback_ = (void*)GetProcAddress(espeakDll_, "espeak_SetSynthCallback");
    fn_SetParameter_ = (void*)GetProcAddress(espeakDll_, "espeak_SetParameter");
    fn_SetVoiceByName_ = (void*)GetProcAddress(espeakDll_, "espeak_SetVoiceByName");
    fn_Synth_ = (void*)GetProcAddress(espeakDll_, "espeak_Synth");
    fn_Synchronize_ = (void*)GetProcAddress(espeakDll_, "espeak_Synchronize");
    fn_Cancel_ = (void*)GetProcAddress(espeakDll_, "espeak_Cancel");
    fn_Terminate_ = (void*)GetProcAddress(espeakDll_, "espeak_Terminate");

    if (!fn_Initialize_ || !fn_SetSynthCallback_ || !fn_Synth_) {
        lastError_ = "Failed to load eSpeak-NG functions";
        FreeLibrary(espeakDll_);
        espeakDll_ = nullptr;
        return false;
    }

    dllLoaded_ = true;
    return true;
#else
    lastError_ = "Dynamic loading not implemented for this platform";
    return false;
#endif
}

//------------------------------------------------------------------------
void ESpeakSynthesizer::unloadDll() {
#ifdef _WIN32
    if (espeakDll_) {
        FreeLibrary(espeakDll_);
        espeakDll_ = nullptr;
    }
    dllLoaded_ = false;
#endif
}

//------------------------------------------------------------------------
int ESpeakSynthesizer::synthCallbackStatic(short* wav, int numsamples, void* events) {
    (void)events;

    std::lock_guard<std::mutex> lock(instanceMutex_);
    if (currentInstance_ && wav && numsamples > 0) {
        currentInstance_->appendSamples(wav, numsamples);
    }

    return 0; // Continue synthesis
}

//------------------------------------------------------------------------
bool ESpeakSynthesizer::initialize(int sampleRate) {
    if (initialized_) {
        return true;
    }

    sampleRate_ = sampleRate;

    // Load DLL
    if (!loadDll()) {
        return false;
    }

    // Initialize eSpeak
    auto initFn = (espeak_Initialize_t)fn_Initialize_;
    int result = initFn(AUDIO_OUTPUT_SYNCHRONOUS, 0, nullptr, 0);
    if (result < 0) {
        lastError_ = "espeak_Initialize failed";
        return false;
    }

    // Set callback
    auto setCallbackFn = (espeak_SetSynthCallback_t)fn_SetSynthCallback_;
    setCallbackFn((void*)synthCallbackStatic);

    // Set default parameters
    setVoice(voice_);
    setRate(rate_);
    setPitch(pitch_);
    setVolume(volume_);

    initialized_ = true;
    return true;
}

//------------------------------------------------------------------------
void ESpeakSynthesizer::shutdown() {
    if (initialized_ && fn_Terminate_) {
        stop();
        auto terminateFn = (espeak_Terminate_t)fn_Terminate_;
        terminateFn();
        initialized_ = false;
    }
}

//------------------------------------------------------------------------
void ESpeakSynthesizer::setVoice(const std::string& voice) {
    voice_ = voice;
    if (initialized_ && fn_SetVoiceByName_) {
        auto setVoiceFn = (espeak_SetVoiceByName_t)fn_SetVoiceByName_;
        setVoiceFn(voice.c_str());
    }
}

//------------------------------------------------------------------------
void ESpeakSynthesizer::setRate(int rate) {
    rate_ = (std::max)(80, (std::min)(450, rate));
    if (initialized_ && fn_SetParameter_) {
        auto setParamFn = (espeak_SetParameter_t)fn_SetParameter_;
        setParamFn(espeakRATE, rate_, 0);
    }
}

//------------------------------------------------------------------------
void ESpeakSynthesizer::setPitch(int pitch) {
    pitch_ = (std::max)(0, (std::min)(99, pitch));
    if (initialized_ && fn_SetParameter_) {
        auto setParamFn = (espeak_SetParameter_t)fn_SetParameter_;
        // Set both base pitch and pitch range for full effect
        setParamFn(espeakPITCH, pitch_, 0);
        // Range 0 = monotone, 50 = normal variation, 100 = max variation
        // Use lower range for more consistent pitch
        setParamFn(espeakRANGE, 20, 0);
    }
}

//------------------------------------------------------------------------
void ESpeakSynthesizer::setVolume(int volume) {
    volume_ = (std::max)(0, (std::min)(200, volume));
    if (initialized_ && fn_SetParameter_) {
        auto setParamFn = (espeak_SetParameter_t)fn_SetParameter_;
        setParamFn(espeakVOLUME, volume_, 0);
    }
}

//------------------------------------------------------------------------
void ESpeakSynthesizer::speak(const std::string& text) {
    if (!initialized_ || text.empty() || !fn_Synth_ || !fn_Synchronize_) {
        return;
    }

    // Set current instance for callback
    {
        std::lock_guard<std::mutex> lock(instanceMutex_);
        currentInstance_ = this;
    }

    speaking_ = true;

    // Synthesize
    auto synthFn = (espeak_Synth_t)fn_Synth_;
    synthFn(text.c_str(), text.length() + 1, 0, POS_CHARACTER, 0,
            espeakCHARS_AUTO | espeakENDPAUSE, nullptr, nullptr);

    // Wait for completion
    auto syncFn = (espeak_Synchronize_t)fn_Synchronize_;
    syncFn();

    speaking_ = false;

    {
        std::lock_guard<std::mutex> lock(instanceMutex_);
        currentInstance_ = nullptr;
    }
}

//------------------------------------------------------------------------
void ESpeakSynthesizer::stop() {
    if (initialized_ && fn_Cancel_) {
        auto cancelFn = (espeak_Cancel_t)fn_Cancel_;
        cancelFn();
        speaking_ = false;
    }
}

//------------------------------------------------------------------------
void ESpeakSynthesizer::appendSamples(const short* samples, int count) {
    std::lock_guard<std::mutex> lock(bufferMutex_);

    for (int i = 0; i < count; ++i) {
        float sample = static_cast<float>(samples[i]) / 32768.0f;
        audioBuffer_.push_back(sample);
    }
}

//------------------------------------------------------------------------
std::vector<float> ESpeakSynthesizer::getAudioSamples() {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    std::vector<float> samples = std::move(audioBuffer_);
    audioBuffer_.clear();
    return samples;
}

//------------------------------------------------------------------------
size_t ESpeakSynthesizer::getAvailableSamples() const {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    return audioBuffer_.size();
}

//------------------------------------------------------------------------
size_t ESpeakSynthesizer::readSamples(float* buffer, size_t maxSamples) {
    std::lock_guard<std::mutex> lock(bufferMutex_);

    size_t samplesToRead = (std::min)(maxSamples, audioBuffer_.size());
    if (samplesToRead > 0) {
        std::memcpy(buffer, audioBuffer_.data(), samplesToRead * sizeof(float));
        audioBuffer_.erase(audioBuffer_.begin(), audioBuffer_.begin() + samplesToRead);
    }

    return samplesToRead;
}

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
