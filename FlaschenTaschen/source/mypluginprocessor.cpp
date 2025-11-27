//------------------------------------------------------------------------
// Copyright(c) 2024 Stratojets.
//------------------------------------------------------------------------

// Prevent Windows min/max macros from interfering with std::min/max
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "mypluginprocessor.h"
#include "myplugincids.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstmessage.h"

#include <fstream>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace {

void logToFile(const std::string& message) {
    std::ofstream logFile("FlaschenTaschenPlugin_log.txt", std::ios::app);
    if (logFile.is_open()) {
        logFile << message << std::endl;
        logFile.close();
    }
}

} // anonymous namespace

using namespace Steinberg;
using namespace FlaschenTaschen;

namespace FTVox {

//------------------------------------------------------------------------
// FTVoxProcessor
//------------------------------------------------------------------------
FTVoxProcessor::FTVoxProcessor()
{
    setControllerClass(kFTVoxControllerUID);
}

//------------------------------------------------------------------------
FTVoxProcessor::~FTVoxProcessor()
{
}

//------------------------------------------------------------------------
tresult PLUGIN_API FTVoxProcessor::initialize(FUnknown* context)
{
    tresult result = AudioEffect::initialize(context);
    if (result != kResultOk)
    {
        return result;
    }

    // Create stereo audio output for TTS
    addAudioOutput(STR16("Stereo Out"), Steinberg::Vst::SpeakerArr::kStereo);

    // Add MIDI event input
    addEventInput(STR16("Event In"), 1);

    // Initialize FlaschenTaschen client
    ftClient_ = std::make_unique<FlaschenTaschenClient>();

    // Initialize TTS synthesizer
    tts_ = std::make_unique<ESpeakSynthesizer>();

    // Initialize pitch shifter
    pitchShifter_ = std::make_unique<WorldPitchShifter>();

    logToFile("FlaschenTaschen plugin initialized");

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API FTVoxProcessor::terminate()
{
    // Disconnect from server
    if (ftClient_) {
        ftClient_->disconnect();
        ftClient_.reset();
    }

    // Shutdown TTS
    if (tts_) {
        tts_->shutdown();
        tts_.reset();
    }

    logToFile("FlaschenTaschen plugin terminated");

    return AudioEffect::terminate();
}

//------------------------------------------------------------------------
tresult PLUGIN_API FTVoxProcessor::setActive(TBool state)
{
    if (state)
    {
        // Activate: Connect to FlaschenTaschen server if config loaded
        if (configLoaded_) {
            std::lock_guard<std::mutex> lock(configMutex_);
            const auto& server = config_.getServerConfig();
            const auto& display = config_.getDisplayConfig();

            ftClient_->setDisplaySize(display.width, display.height);
            ftClient_->setOffset(display.offsetX, display.offsetY);
            ftClient_->setLayer(display.layer);

            if (ftClient_->connect(server.ip, server.port)) {
                ftConnected_ = true;
                logToFile("Connected to FlaschenTaschen server: " + server.ip + ":" + std::to_string(server.port));

                // Clear display
                ftClient_->clear();
                ftClient_->send();
            } else {
                logToFile("Failed to connect to FlaschenTaschen server: " + ftClient_->getLastError());
            }
        }

        // Initialize TTS at eSpeak native rate (22050 Hz)
        if (tts_ && !tts_->isInitialized()) {
            ttsSampleRate_ = 22050;  // eSpeak's preferred rate
            if (tts_->initialize(ttsSampleRate_)) {
                ttsSampleRate_ = tts_->getSampleRate();  // Get actual rate
                logToFile("TTS initialized at sample rate: " + std::to_string(ttsSampleRate_));
                logToFile("Output sample rate: " + std::to_string(sampleRate_) + " (will resample)");
            } else {
                logToFile("TTS initialization failed: " + tts_->getLastError());
            }
        }

        // Initialize pitch shifter at TTS sample rate
        if (pitchShifter_) {
            pitchShifter_->initialize(ttsSampleRate_);
            logToFile("Pitch shifter initialized at " + std::to_string(ttsSampleRate_) + " Hz");
        }
    }
    else
    {
        // Deactivate: Disconnect
        if (ftClient_) {
            ftClient_->disconnect();
            ftConnected_ = false;
        }
    }

    return AudioEffect::setActive(state);
}

//------------------------------------------------------------------------
tresult PLUGIN_API FTVoxProcessor::process(Vst::ProcessData& data)
{
    // Process parameter changes
    if (data.inputParameterChanges)
    {
        int32 numParamsChanged = data.inputParameterChanges->getParameterCount();
        for (int32 index = 0; index < numParamsChanged; index++)
        {
            if (auto* paramQueue = data.inputParameterChanges->getParameterData(index))
            {
                Vst::ParamValue value;
                int32 sampleOffset;
                int32 numPoints = paramQueue->getPointCount();
                if (paramQueue->getPoint(numPoints - 1, sampleOffset, value) == kResultTrue)
                {
                    switch (paramQueue->getParameterId())
                    {
                        case kParamFontScale:
                            fontScale_ = static_cast<float>(value);
                            font_.setScale(1 + static_cast<int>(value * 4)); // 1-5 scale
                            break;
                        case kParamColorR:
                            colorR_ = static_cast<float>(value);
                            break;
                        case kParamColorG:
                            colorG_ = static_cast<float>(value);
                            break;
                        case kParamColorB:
                            colorB_ = static_cast<float>(value);
                            break;
                        case kParamTTSEnabled:
                            ttsEnabled_ = value > 0.5;
                            break;
                        case kParamTTSRate:
                            ttsRate_ = static_cast<float>(value);
                            if (tts_ && tts_->isInitialized()) {
                                tts_->setRate(80 + static_cast<int>(value * 370)); // 80-450
                            }
                            break;
                        case kParamTTSPitch:
                            ttsPitch_ = static_cast<float>(value);
                            if (tts_ && tts_->isInitialized()) {
                                tts_->setPitch(static_cast<int>(value * 99)); // 0-99
                            }
                            break;
                        case kParamTTSVolume:
                            ttsVolume_ = static_cast<float>(value);
                            if (tts_ && tts_->isInitialized()) {
                                tts_->setVolume(static_cast<int>(value * 200)); // 0-200
                            }
                            break;
                        case kParamPitchShiftEnabled:
                            pitchShiftEnabled_ = value > 0.5;
                            break;
                        case kParamOctaveOffset:
                            // Map 0-1 to -3 to +3 octaves
                            octaveOffset_ = static_cast<int>(std::round(value * 6.0 - 3.0));
                            break;
                    }
                }
            }
        }
    }

    // Process MIDI events
    if (data.inputEvents)
    {
        for (int32 i = 0; i < data.inputEvents->getEventCount(); i++)
        {
            Vst::Event event;
            if (data.inputEvents->getEvent(i, event) == kResultOk)
            {
                switch (event.type)
                {
                    case Vst::Event::kNoteOnEvent:
                        handleNoteOn(event.noteOn.pitch, static_cast<int>(event.noteOn.velocity * 127));
                        break;

                    case Vst::Event::kNoteOffEvent:
                        handleNoteOff(event.noteOff.pitch);
                        break;

                    default:
                        break;
                }
            }
        }
    }

    // Process audio output (TTS)
    if (data.numSamples > 0 && data.numOutputs > 0)
    {
        // Get output buffers
        Vst::Sample32** outputs = data.outputs[0].channelBuffers32;
        int numChannels = data.outputs[0].numChannels;

        // Process TTS audio
        processTTSAudio(outputs, numChannels, data.numSamples);
    }

    return kResultOk;
}

//------------------------------------------------------------------------
void FTVoxProcessor::handleNoteOn(int noteNumber, int velocity)
{
    (void)velocity;

    if (!configLoaded_) {
        return;
    }

    std::string syllable;
    {
        std::lock_guard<std::mutex> lock(configMutex_);
        syllable = config_.getSyllableForNote(noteNumber);
    }

    if (!syllable.empty()) {
        currentNoteNumber_ = noteNumber;
        currentSyllable_ = syllable;

        logToFile("Note ON: " + std::to_string(noteNumber) + " -> syllable: " + syllable);

        // Update LED display
        updateDisplay(syllable);

        // Speak syllable via TTS with pitch shifting
        if (ttsEnabled_) {
            speakSyllable(syllable, noteNumber);
        }
    }
}

//------------------------------------------------------------------------
void FTVoxProcessor::handleNoteOff(int noteNumber)
{
    // Only clear if this is the currently displayed note
    if (currentNoteNumber_ == noteNumber) {
        currentNoteNumber_ = -1;
        currentSyllable_.clear();

        logToFile("Note OFF: " + std::to_string(noteNumber));

        // Clear display (show nothing or keep last syllable based on preference)
        // For now, we keep the last syllable displayed
    }
}

//------------------------------------------------------------------------
void FTVoxProcessor::updateDisplay(const std::string& syllable)
{
    if (!ftConnected_ || !ftClient_) {
        return;
    }

    // Get display config
    const auto& display = config_.getDisplayConfig();

    // Create color from parameters
    Color textColor(
        static_cast<uint8_t>(colorR_ * 255),
        static_cast<uint8_t>(colorG_ * 255),
        static_cast<uint8_t>(colorB_ * 255)
    );
    Color bgColor(display.bgColorR, display.bgColorG, display.bgColorB);

    // Clear and render
    ftClient_->clear(bgColor);
    font_.renderTextCenteredFull(*ftClient_, syllable, textColor, bgColor);

    // Send to server
    if (!ftClient_->send()) {
        logToFile("Failed to send frame: " + ftClient_->getLastError());
    }
}

//------------------------------------------------------------------------
void FTVoxProcessor::speakSyllable(const std::string& syllable, int midiNote)
{
    if (!tts_ || !tts_->isInitialized()) {
        return;
    }

    // Stop any current speech
    tts_->stop();

    // Generate TTS audio synchronously
    tts_->speak(syllable);

    // Get generated audio samples
    auto samples = tts_->getAudioSamples();
    if (samples.empty()) {
        return;
    }

    logToFile("TTS generated " + std::to_string(samples.size()) + " samples");

    // Apply pitch shifting based on MIDI note + octave offset
    if (pitchShiftEnabled_ && pitchShifter_) {
        int pitchNote = midiNote + (octaveOffset_ * 12);
        if (pitchNote < 0) pitchNote = 0;
        if (pitchNote > 127) pitchNote = 127;
        double targetFreq = WorldPitchShifter::midiNoteToFrequency(pitchNote);
        samples = pitchShifter_->processToFrequency(samples, targetFreq);
        logToFile("Pitch shifted to " + std::to_string(targetFreq) + " Hz (MIDI " + std::to_string(pitchNote) + ")");
    }

    // Resample from TTS rate to output rate
    int outputRate = static_cast<int>(sampleRate_);
    if (ttsSampleRate_ != outputRate) {
        samples = resample(samples, ttsSampleRate_, outputRate);
        logToFile("Resampled " + std::to_string(ttsSampleRate_) + " -> " + std::to_string(outputRate) + " Hz");
    }

    // Add to playback buffer
    {
        std::lock_guard<std::mutex> lock(ttsBufferMutex_);
        ttsAudioBuffer_.insert(ttsAudioBuffer_.end(), samples.begin(), samples.end());
    }
}

//------------------------------------------------------------------------
void FTVoxProcessor::processTTSAudio(Vst::Sample32** outputs, int numChannels, int numSamples)
{
    std::lock_guard<std::mutex> lock(ttsBufferMutex_);

    int samplesAvailable = static_cast<int>(ttsAudioBuffer_.size());
    int framesToRead = (std::min)(numSamples, samplesAvailable);

    for (int frame = 0; frame < numSamples; ++frame) {
        float sample = 0.0f;

        if (frame < framesToRead && frame < static_cast<int>(ttsAudioBuffer_.size())) {
            sample = ttsAudioBuffer_[frame];
        }

        // Write to all channels (mono to stereo)
        for (int ch = 0; ch < numChannels; ++ch) {
            outputs[ch][frame] = sample;
        }
    }

    // Remove consumed samples
    if (framesToRead > 0 && framesToRead <= static_cast<int>(ttsAudioBuffer_.size())) {
        ttsAudioBuffer_.erase(ttsAudioBuffer_.begin(), ttsAudioBuffer_.begin() + framesToRead);
    }
}

//------------------------------------------------------------------------
std::vector<float> FTVoxProcessor::resample(const std::vector<float>& input, int inputRate, int outputRate)
{
    if (inputRate == outputRate || input.empty()) {
        return input;
    }

    double ratio = static_cast<double>(outputRate) / inputRate;
    size_t outputSize = static_cast<size_t>(input.size() * ratio);
    std::vector<float> output(outputSize);

    for (size_t i = 0; i < outputSize; ++i) {
        double srcPos = i / ratio;
        size_t srcIndex = static_cast<size_t>(srcPos);
        double frac = srcPos - srcIndex;

        if (srcIndex + 1 < input.size()) {
            // Linear interpolation
            output[i] = static_cast<float>(input[srcIndex] * (1.0 - frac) + input[srcIndex + 1] * frac);
        } else if (srcIndex < input.size()) {
            output[i] = input[srcIndex];
        } else {
            output[i] = 0.0f;
        }
    }

    return output;
}

//------------------------------------------------------------------------
tresult PLUGIN_API FTVoxProcessor::notify(Vst::IMessage* message)
{
    if (!message)
        return kInvalidArgument;

    if (strcmp(message->getMessageID(), "MappingFile") == 0)
    {
        // Get the file path from the message
        const void* data = nullptr;
        Steinberg::uint32 size = 0;
        if (message->getAttributes()->getBinary("Path", data, size) == kResultOk && data && size > 0)
        {
            // Convert from UTF-16 to UTF-8
            const Steinberg::char16* path16 = static_cast<const Steinberg::char16*>(data);
            std::string path;
            while (*path16) {
                path += static_cast<char>(*path16++);
            }

            logToFile("Received mapping file path: " + path);
            loadMappingFile(path);

            // Reconnect to server with new config
            if (configLoaded_ && ftClient_) {
                ftClient_->disconnect();
                const auto& server = config_.getServerConfig();
                const auto& display = config_.getDisplayConfig();
                ftClient_->setDisplaySize(display.width, display.height);
                ftClient_->setOffset(display.offsetX, display.offsetY);
                ftClient_->setLayer(display.layer);
                ftClient_->setFlipHorizontal(display.flipHorizontal);
                font_.setMirrorGlyph(display.mirrorGlyph);

                if (ftClient_->connect(server.ip, server.port)) {
                    ftConnected_ = true;
                    logToFile("Reconnected to FlaschenTaschen server");
                }
            }
        }
        return kResultOk;
    }

    return AudioEffect::notify(message);
}

//------------------------------------------------------------------------
bool FTVoxProcessor::loadMappingFile(const std::string& filePath)
{
    std::lock_guard<std::mutex> lock(configMutex_);

    if (config_.loadFromFile(filePath)) {
        configFilePath_ = filePath;
        configLoaded_ = true;

        logToFile("Loaded mapping file: " + filePath);
        logToFile("  Server: " + config_.getServerConfig().ip + ":" + std::to_string(config_.getServerConfig().port));
        logToFile("  Syllables: " + std::to_string(config_.getSyllables().size()));
        logToFile("  Note mappings: " + std::to_string(config_.getNoteMappings().size()));

        // Update font scale from display config
        font_.setScale(1);

        return true;
    }
    else {
        logToFile("Failed to load mapping file: " + config_.getLastError());
        return false;
    }
}

//------------------------------------------------------------------------
std::string FTVoxProcessor::getCurrentSyllable() const
{
    return currentSyllable_;
}

//------------------------------------------------------------------------
tresult PLUGIN_API FTVoxProcessor::setupProcessing(Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate;

    // Update TTS sample rate if already initialized
    if (tts_ && tts_->isInitialized()) {
        tts_->shutdown();
        tts_->initialize(static_cast<int>(sampleRate_));
    }

    return AudioEffect::setupProcessing(newSetup);
}

//------------------------------------------------------------------------
tresult PLUGIN_API FTVoxProcessor::canProcessSampleSize(int32 symbolicSampleSize)
{
    if (symbolicSampleSize == Vst::kSample32)
        return kResultTrue;

    return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API FTVoxProcessor::setState(IBStream* state)
{
    IBStreamer streamer(state, kLittleEndian);

    // Read config file path
    int32 pathLength = 0;
    if (streamer.readInt32(pathLength) == false)
        return kResultFalse;

    if (pathLength > 0) {
        std::vector<char> pathBuffer(pathLength + 1, 0);
        if (streamer.readRaw(pathBuffer.data(), pathLength) == false)
            return kResultFalse;

        std::string filePath(pathBuffer.data());
        loadMappingFile(filePath);
    }

    // Read parameters
    float fontScale, r, g, b;
    int32 ttsEnabled;
    float ttsRate, ttsPitch, ttsVolume;

    if (streamer.readFloat(fontScale) == false) return kResultOk; // Old state without these
    fontScale_ = fontScale;
    font_.setScale(1 + static_cast<int>(fontScale * 4));

    if (streamer.readFloat(r) == false) return kResultOk;
    if (streamer.readFloat(g) == false) return kResultOk;
    if (streamer.readFloat(b) == false) return kResultOk;
    colorR_ = r;
    colorG_ = g;
    colorB_ = b;

    if (streamer.readInt32(ttsEnabled) == false) return kResultOk;
    ttsEnabled_ = ttsEnabled != 0;

    if (streamer.readFloat(ttsRate) == false) return kResultOk;
    if (streamer.readFloat(ttsPitch) == false) return kResultOk;
    if (streamer.readFloat(ttsVolume) == false) return kResultOk;
    ttsRate_ = ttsRate;
    ttsPitch_ = ttsPitch;
    ttsVolume_ = ttsVolume;

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API FTVoxProcessor::getState(IBStream* state)
{
    IBStreamer streamer(state, kLittleEndian);

    // Write config file path
    int32 pathLength = static_cast<int32>(configFilePath_.length());
    streamer.writeInt32(pathLength);
    if (pathLength > 0) {
        streamer.writeRaw(configFilePath_.c_str(), pathLength);
    }

    // Write parameters
    streamer.writeFloat(fontScale_.load());
    streamer.writeFloat(colorR_.load());
    streamer.writeFloat(colorG_.load());
    streamer.writeFloat(colorB_.load());
    streamer.writeInt32(ttsEnabled_ ? 1 : 0);
    streamer.writeFloat(ttsRate_.load());
    streamer.writeFloat(ttsPitch_.load());
    streamer.writeFloat(ttsVolume_.load());

    return kResultOk;
}

//------------------------------------------------------------------------
} // namespace FTVox
