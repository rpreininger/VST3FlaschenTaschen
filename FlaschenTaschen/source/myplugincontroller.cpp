//------------------------------------------------------------------------
// Copyright(c) 2024 Stratojets.
//------------------------------------------------------------------------

#include "myplugincontroller.h"
#include "mypluginprocessor.h"
#include "myplugincids.h"
#include "base/source/fstreamer.h"
#include "base/source/fstring.h"
#include "vstgui/lib/cfileselector.h"
#include "vstgui/lib/cframe.h"

using namespace Steinberg;

namespace FTVox {

//------------------------------------------------------------------------
// FTVoxController Implementation
//------------------------------------------------------------------------
tresult PLUGIN_API FTVoxController::initialize(FUnknown* context)
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

    // Pitch Shift Enabled (on/off)
    parameters.addParameter(STR16("Pitch Shift"), nullptr, 1,
                            1.0, // default on
                            Vst::ParameterInfo::kCanAutomate | Vst::ParameterInfo::kIsList,
                            kParamPitchShiftEnabled);

    // Octave Offset (-3 to +3, mapped to 0-1)
    parameters.addParameter(STR16("Octave Offset"), STR16("oct"), 0,
                            0.5, // default 0 (middle)
                            Vst::ParameterInfo::kCanAutomate,
                            kParamOctaveOffset);

    return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API FTVoxController::terminate()
{
    return EditControllerEx1::terminate();
}

//------------------------------------------------------------------------
tresult PLUGIN_API FTVoxController::setComponentState(IBStream* state)
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
tresult PLUGIN_API FTVoxController::setState(IBStream* state)
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
tresult PLUGIN_API FTVoxController::getState(IBStream* state)
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
IPlugView* PLUGIN_API FTVoxController::createView(FIDString name)
{
    if (FIDStringsEqual(name, Vst::ViewType::kEditor))
    {
        auto* view = new VSTGUI::VST3Editor(this, "view", "myplugineditor.uidesc");
        return view;
    }
    return nullptr;
}

//------------------------------------------------------------------------
// Custom button listener for file browser
class FileBrowserButtonListener : public VSTGUI::IControlListener
{
public:
    FileBrowserButtonListener(FTVoxController* controller, VSTGUI::CFrame* frame)
        : controller_(controller), frame_(frame) {}

    void valueChanged(VSTGUI::CControl* control) override
    {
        if (control->getValue() > 0.5f && controller_ && frame_) {
            controller_->openFileBrowser(frame_);
        }
    }

private:
    FTVoxController* controller_;
    VSTGUI::CFrame* frame_;
};

//------------------------------------------------------------------------
VSTGUI::CView* FTVoxController::createCustomView(
    VSTGUI::UTF8StringPtr name,
    const VSTGUI::UIAttributes& attributes,
    const VSTGUI::IUIDescription* description,
    VSTGUI::VST3Editor* editor)
{
    (void)attributes;
    (void)description;

    if (name && strcmp(name, "FileBrowserButton") == 0)
    {
        // Create a text button for file browsing
        auto button = new VSTGUI::CTextButton(VSTGUI::CRect(0, 0, 100, 25), nullptr, -1, "Browse...");
        button->setListener(new FileBrowserButtonListener(this, editor->getFrame()));
        return button;
    }

    if (name && strcmp(name, "FilePathLabel") == 0)
    {
        // Create a label to show current file path
        auto label = new VSTGUI::CTextLabel(VSTGUI::CRect(0, 0, 300, 20));
        label->setText(mappingFilePath_.empty() ? "(no file selected)" : mappingFilePath_.c_str());
        label->setBackColor(VSTGUI::CColor(40, 40, 40, 255));
        label->setFontColor(VSTGUI::CColor(200, 200, 200, 255));
        label->setHoriAlign(VSTGUI::CHoriTxtAlign::kLeftText);
        filePathLabel_ = label;
        return label;
    }

    return nullptr;
}

//------------------------------------------------------------------------
void FTVoxController::willClose(VSTGUI::VST3Editor* editor)
{
    (void)editor;
}

//------------------------------------------------------------------------
tresult PLUGIN_API FTVoxController::notify(Vst::IMessage* message)
{
    if (!message)
        return kInvalidArgument;

    // Handle messages from processor if needed

    return EditControllerEx1::notify(message);
}

//------------------------------------------------------------------------
void FTVoxController::sendMappingFilePath(const std::string& path)
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

    // Update file path label if exists
    if (filePathLabel_) {
        // Extract just the filename from the path
        std::string filename = path;
        size_t lastSlash = path.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            filename = path.substr(lastSlash + 1);
        }
        filePathLabel_->setText(VSTGUI::UTF8String(filename));
    }
}

//------------------------------------------------------------------------
void FTVoxController::openFileBrowser(VSTGUI::CFrame* frame)
{
    if (!frame)
        return;

    auto fileSelector = VSTGUI::CNewFileSelector::create(frame, VSTGUI::CNewFileSelector::kSelectFile);
    if (fileSelector)
    {
        fileSelector->setTitle("Select Mapping XML File");
        fileSelector->setDefaultExtension(VSTGUI::CFileExtension("XML Files", "xml"));
        fileSelector->addFileExtension(VSTGUI::CFileExtension("All Files", "*"));

        // Set initial directory if we have a previous path
        if (!mappingFilePath_.empty()) {
            std::string dir = mappingFilePath_;
            size_t lastSlash = dir.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                dir = dir.substr(0, lastSlash);
                fileSelector->setInitialDirectory(dir.c_str());
            }
        }

        fileSelector->run([this](VSTGUI::CNewFileSelector* selector) {
            if (selector->getNumSelectedFiles() > 0) {
                VSTGUI::UTF8StringPtr selectedFile = selector->getSelectedFile(0);
                if (selectedFile) {
                    sendMappingFilePath(std::string(selectedFile));
                }
            }
        });

        fileSelector->forget();
    }
}

//------------------------------------------------------------------------
} // namespace FTVox
