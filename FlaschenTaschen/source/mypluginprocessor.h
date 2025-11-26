//------------------------------------------------------------------------
// Copyright(c) 2024 Stratojets.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "MappingConfig.h"
#include "FlaschenTaschenClient.h"
#include "BitmapFont.h"
#include "ESpeakSynthesizer.h"

#include <memory>
#include <string>
#include <atomic>
#include <mutex>

namespace FlaschenTaschen {

// Parameter IDs
enum ParameterIDs : Steinberg::Vst::ParamID {
    kParamFontScale = 100,
    kParamColorR,
    kParamColorG,
    kParamColorB,
    kParamTTSEnabled,
    kParamTTSRate,
    kParamTTSPitch,
    kParamTTSVolume,
};

//------------------------------------------------------------------------
//  FlaschenTaschenProcessor
//------------------------------------------------------------------------
class FlaschenTaschenProcessor : public Steinberg::Vst::AudioEffect
{
public:
    FlaschenTaschenProcessor();
    ~FlaschenTaschenProcessor() SMTG_OVERRIDE;

    // Create function
    static Steinberg::FUnknown* createInstance(void* /*context*/)
    {
        return (Steinberg::Vst::IAudioProcessor*)new FlaschenTaschenProcessor;
    }

    //--- ---------------------------------------------------------------------
    // AudioEffect overrides:
    //--- ---------------------------------------------------------------------
    /** Called at first after constructor */
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;

    /** Called at the end before destructor */
    Steinberg::tresult PLUGIN_API terminate() SMTG_OVERRIDE;

    /** Switch the Plug-in on/off */
    Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) SMTG_OVERRIDE;

    /** Will be called before any process call */
    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& newSetup) SMTG_OVERRIDE;

    /** Asks if a given sample size is supported see SymbolicSampleSizes. */
    Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;

    /** Here we go...the process call */
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;

    /** For persistence */
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) SMTG_OVERRIDE;

    // Load mapping configuration from file
    bool loadMappingFile(const std::string& filePath);

    // Get current displayed syllable
    std::string getCurrentSyllable() const;

//------------------------------------------------------------------------
protected:
    // Handle MIDI note on event
    void handleNoteOn(int noteNumber, int velocity);

    // Handle MIDI note off event
    void handleNoteOff(int noteNumber);

    // Send current syllable to LED display
    void updateDisplay(const std::string& syllable);

    // Speak current syllable using TTS
    void speakSyllable(const std::string& syllable);

    // Process TTS audio output
    void processTTSAudio(Steinberg::Vst::Sample32** outputs, int numChannels, int numSamples);

private:
    // Configuration
    MappingConfig config_;
    std::string configFilePath_;
    std::atomic<bool> configLoaded_{false};
    mutable std::mutex configMutex_;

    // FlaschenTaschen client
    std::unique_ptr<FlaschenTaschenClient> ftClient_;
    std::atomic<bool> ftConnected_{false};

    // Font renderer
    BitmapFont font_;

    // TTS synthesizer
    std::unique_ptr<ESpeakSynthesizer> tts_;

    // Current state
    std::string currentSyllable_;
    std::atomic<int> currentNoteNumber_{-1};

    // Parameters
    std::atomic<float> fontScale_{1.0f};
    std::atomic<float> colorR_{1.0f};
    std::atomic<float> colorG_{1.0f};
    std::atomic<float> colorB_{1.0f};
    std::atomic<bool> ttsEnabled_{true};
    std::atomic<float> ttsRate_{0.5f};   // Normalized 0-1
    std::atomic<float> ttsPitch_{0.5f};  // Normalized 0-1
    std::atomic<float> ttsVolume_{0.5f}; // Normalized 0-1

    // Audio processing
    double sampleRate_ = 44100.0;
};

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
