//------------------------------------------------------------------------
// Copyright(c) 2024 Stratojets.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"

#include <string>

namespace FlaschenTaschen {

//------------------------------------------------------------------------
//  FlaschenTaschenController
//------------------------------------------------------------------------
class FlaschenTaschenController : public Steinberg::Vst::EditControllerEx1,
                                   public VSTGUI::VST3EditorDelegate
{
public:
//------------------------------------------------------------------------
    FlaschenTaschenController() = default;
    ~FlaschenTaschenController() SMTG_OVERRIDE = default;

    // Create function
    static Steinberg::FUnknown* createInstance(void* /*context*/)
    {
        return (Steinberg::Vst::IEditController*)new FlaschenTaschenController;
    }

    //--- from IPluginBase -----------------------------------------------
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API terminate() SMTG_OVERRIDE;

    //--- from EditController --------------------------------------------
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) SMTG_OVERRIDE;

    //--- from VST3EditorDelegate ----------------------------------------
    VSTGUI::CView* createCustomView(VSTGUI::UTF8StringPtr name,
                                     const VSTGUI::UIAttributes& attributes,
                                     const VSTGUI::IUIDescription* description,
                                     VSTGUI::VST3Editor* editor) SMTG_OVERRIDE;

    void willClose(VSTGUI::VST3Editor* editor) SMTG_OVERRIDE;

    //--- IMidiMapping ---------------------------------------------------
    // Steinberg::tresult PLUGIN_API getMidiControllerAssignment(
    //     Steinberg::int32 busIndex, Steinberg::int16 channel,
    //     Steinberg::Vst::CtrlNumber midiControllerNumber,
    //     Steinberg::Vst::ParamID& id) SMTG_OVERRIDE;

    //--- Message handling -----------------------------------------------
    Steinberg::tresult PLUGIN_API notify(Steinberg::Vst::IMessage* message) SMTG_OVERRIDE;

    // Send message to processor
    void sendMappingFilePath(const std::string& path);

    //---Interface---------
    DEFINE_INTERFACES
        // DEF_INTERFACE(Steinberg::Vst::IMidiMapping)
    END_DEFINE_INTERFACES(EditController)
    DELEGATE_REFCOUNT(EditController)

//------------------------------------------------------------------------
protected:
    std::string mappingFilePath_;
};

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
