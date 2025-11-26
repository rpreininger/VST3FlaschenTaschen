//------------------------------------------------------------------------
// Copyright(c) 2024 Stratojets.
//------------------------------------------------------------------------

#include "myplugincontroller.h"
#include "mypluginprocessor.h"
#include "myplugincids.h"
#include "base/source/fstreamer.h"
#include "base/source/fstring.h"

using namespace Steinberg;

namespace FlaschenTaschen {

//------------------------------------------------------------------------
// FlaschenTaschenController Implementation
//------------------------------------------------------------------------
tresult PLUGIN_API FlaschenTaschenController::initialize(FUnknown* context)
{
    tresult result = EditControllerEx1::initialize(context);
    if (result != kResultOk)
    {
        return result;
    }

    // Register parameters

    // Font Scale (0-1 maps to scale 1-5)
    parameters.addParameter(STR16("Font Scale"), STR16("%"), 0,
                            0.0, // default
                            Vst::ParameterInfo::kCanAutomate,
                            kParamFontScale);

    // Color R
    parameters.addParameter(STR16("Color R"), STR16("%"), 0,
                            1.0, // default (white)
                            Vst::ParameterInfo::kCanAutomate,
                            kParamColorR);

    // Color G
    parameters.addParameter(STR16("Color G"), STR16("%"), 0,
                            1.0,
                            Vst::ParameterInfo::kCanAutomate,
                            kParamColorG);

    // Color B
    parameters.addParameter(STR16("Color B"), STR16("%"), 0,
                            1.0,
                            Vst::ParameterInfo::kCanAutomate,
                            kParamColorB);

    // TTS Enabled (on/off)
    parameters.addParameter(STR16("TTS Enabled"), nullptr, 1,
                            1.0, // default on
                            Vst::ParameterInfo::kCanAutomate | Vst::ParameterInfo::kIsList,
                            kParamTTSEnabled);

    // TTS Rate
    parameters.addParameter(STR16("TTS Rate"), STR16("%"), 0,
                            0.5, // default (middle)
                            Vst::ParameterInfo::kCanAutomate,
                            kParamTTSRate);

    // TTS Pitch
    parameters.addParameter(STR16("TTS Pitch"), STR16("%"), 0,
                            0.5,
                            Vst::ParameterInfo::kCanAutomate,
                            kParamTTSPitch);

    // TTS Volume
    parameters.addParameter(STR16("TTS Volume"), STR16("%"), 0,
                            0.5,
                            Vst::ParameterInfo::kCanAutomate,
                            kParamTTSVolume);

    return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API FlaschenTaschenController::terminate()
{
    return EditControllerEx1::terminate();
}

//------------------------------------------------------------------------
tresult PLUGIN_API FlaschenTaschenController::setComponentState(IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer(state, kLittleEndian);

    // Read config file path (skip it, processor handles it)
    int32 pathLength = 0;
    if (streamer.readInt32(pathLength) == false)
        return kResultFalse;

    if (pathLength > 0) {
        std::vector<char> pathBuffer(pathLength + 1, 0);
        if (streamer.readRaw(pathBuffer.data(), pathLength) == false)
            return kResultFalse;
        mappingFilePath_ = std::string(pathBuffer.data());
    }

    // Read and set parameters
    float fontScale, r, g, b;
    int32 ttsEnabled;
    float ttsRate, ttsPitch, ttsVolume;

    if (streamer.readFloat(fontScale) == false) return kResultOk;
    setParamNormalized(kParamFontScale, fontScale);

    if (streamer.readFloat(r) == false) return kResultOk;
    if (streamer.readFloat(g) == false) return kResultOk;
    if (streamer.readFloat(b) == false) return kResultOk;
    setParamNormalized(kParamColorR, r);
    setParamNormalized(kParamColorG, g);
    setParamNormalized(kParamColorB, b);

    if (streamer.readInt32(ttsEnabled) == false) return kResultOk;
    setParamNormalized(kParamTTSEnabled, ttsEnabled ? 1.0 : 0.0);

    if (streamer.readFloat(ttsRate) == false) return kResultOk;
    if (streamer.readFloat(ttsPitch) == false) return kResultOk;
    if (streamer.readFloat(ttsVolume) == false) return kResultOk;
    setParamNormalized(kParamTTSRate, ttsRate);
    setParamNormalized(kParamTTSPitch, ttsPitch);
    setParamNormalized(kParamTTSVolume, ttsVolume);

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API FlaschenTaschenController::setState(IBStream* state)
{
    IBStreamer streamer(state, kLittleEndian);

    // Read mapping file path from controller state
    int32 pathLength = 0;
    if (streamer.readInt32(pathLength)) {
        if (pathLength > 0) {
            std::vector<char> pathBuffer(pathLength + 1, 0);
            if (streamer.readRaw(pathBuffer.data(), pathLength)) {
                mappingFilePath_ = std::string(pathBuffer.data());
            }
        }
    }

    return kResultTrue;
}

//------------------------------------------------------------------------
tresult PLUGIN_API FlaschenTaschenController::getState(IBStream* state)
{
    IBStreamer streamer(state, kLittleEndian);

    // Save mapping file path to controller state
    int32 pathLength = static_cast<int32>(mappingFilePath_.length());
    streamer.writeInt32(pathLength);
    if (pathLength > 0) {
        streamer.writeRaw(mappingFilePath_.c_str(), pathLength);
    }

    return kResultTrue;
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API FlaschenTaschenController::createView(FIDString name)
{
    if (FIDStringsEqual(name, Vst::ViewType::kEditor))
    {
        auto* view = new VSTGUI::VST3Editor(this, "view", "myplugineditor.uidesc");
        return view;
    }
    return nullptr;
}

//------------------------------------------------------------------------
VSTGUI::CView* FlaschenTaschenController::createCustomView(
    VSTGUI::UTF8StringPtr name,
    const VSTGUI::UIAttributes& attributes,
    const VSTGUI::IUIDescription* description,
    VSTGUI::VST3Editor* editor)
{
    (void)attributes;
    (void)description;
    (void)editor;
    (void)name;

    // Custom views can be created here if needed
    return nullptr;
}

//------------------------------------------------------------------------
void FlaschenTaschenController::willClose(VSTGUI::VST3Editor* editor)
{
    (void)editor;
}

//------------------------------------------------------------------------
tresult PLUGIN_API FlaschenTaschenController::notify(Vst::IMessage* message)
{
    if (!message)
        return kInvalidArgument;

    // Handle messages from processor if needed

    return EditControllerEx1::notify(message);
}

//------------------------------------------------------------------------
void FlaschenTaschenController::sendMappingFilePath(const std::string& path)
{
    mappingFilePath_ = path;

    // Send message to processor
    if (auto message = allocateMessage())
    {
        message->setMessageID("MappingFile");
        // Convert path to Steinberg String16 for VST3 API
        Steinberg::String pathStr(path.c_str());
        message->getAttributes()->setString("Path", pathStr.text16());
        sendMessage(message);
        message->release();
    }
}

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
