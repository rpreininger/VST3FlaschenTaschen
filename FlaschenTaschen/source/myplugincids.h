//------------------------------------------------------------------------
// Copyright(c) 2024 Stratojets.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace FlaschenTaschen {
//------------------------------------------------------------------------
static const Steinberg::FUID kFlaschenTaschenProcessorUID (0x4F540AFC, 0x9B7A52EC, 0x879BF164, 0xFBBFFA53);
static const Steinberg::FUID kFlaschenTaschenControllerUID (0x663075D2, 0x7E925171, 0xBEA68036, 0x5B54E6EC);

#define FlaschenTaschenVST3Category "Instrument"

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
