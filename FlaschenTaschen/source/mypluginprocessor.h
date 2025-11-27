//------------------------------------------------------------------------
// Copyright(c) 2024 Stratojets.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "MappingConfig.h"
#include "FlaschenTaschenClient.h"
#include "BitmapFont.h"
#include "ESpeakSynthesizer.h"
#include "WorldPitchShifter.h"

#include <memory>
#include <string>
#include <atomic>
#include <mutex>
#include <vector>

namespace FTVox {

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
    kParamPitchShiftEnabled,
    kParamOctaveOffset,
};

//------------------------------------------------------------------------
//  FTVoxProcessor
//------------------------------------------------------------------------
class FTVoxProcessor : public Steinberg::Vst::AudioEffect
{
public:
    FTVoxProcessor();
    ~FTVoxProcessor() SMTG_OVERRIDE;

    // Create function
    static Steinberg::FUnknown* createInstance(void* /*context*/)
    {
        return (Steinberg::Vst::IAudioProcessor*)new FTVoxProcessor;
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

    // Speak current syllable using TTS with pitch shifting
    void speakSyllable(const std::string& syllable, int midiNote);

    // Process TTS audio output
    void processTTSAudio(Steinberg::Vst::Sample32** outputs, int numChannels, int numSamples);

    // Resample audio from one sample rate to another
    std::vector<float> resample(const std::vector<float>& input, int inputRate, int outputRate);

    // Handle message from controller (for file path)
    Steinberg::tresult PLUGIN_API notify(Steinberg::Vst::IMessage* message) SMTG_OVERRIDE;

private:
    // Configuration
    FlaschenTaschen::MappingConfig config_;
    std::string configFilePath_;
    std::atomic<bool> configLoaded_{false};
    mutable std::mutex configMutex_;

    // FlaschenTaschen client
    std::unique_ptr<FlaschenTaschen::FlaschenTaschenClient> ftClient_;
    std::atomic<bool> ftConnected_{false};

    // Font renderer
    FlaschenTaschen::BitmapFont font_;

    // TTS synthesizer
    std::unique_ptr<FlaschenTaschen::ESpeakSynthesizer> tts_;
    int ttsSampleRate_ = 22050;  // eSpeak native rate

    // World pitch shifter
    std::unique_ptr<FlaschenTaschen::WorldPitchShifter> pitchShifter_;

    // TTS audio buffer for playback
    std::vector<float> ttsAudioBuffer_;
    std::mutex ttsBufferMutex_;

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
    std::atomic<bool> pitchShiftEnabled_{true};
    std::atomic<int> octaveOffset_{0};   // -3 to +3

    // Audio processing
    double sampleRate_ = 44100.0;
};

//------------------------------------------------------------------------
} // namespace FTVox
