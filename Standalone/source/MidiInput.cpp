//------------------------------------------------------------------------
// FlaschenTaschen Standalone - Windows MIDI Input
//------------------------------------------------------------------------

#include "MidiInput.h"

#pragma comment(lib, "winmm.lib")

namespace FlaschenTaschen {

//------------------------------------------------------------------------
MidiInput::MidiInput() {
}

//------------------------------------------------------------------------
MidiInput::~MidiInput() {
    close();
}

//------------------------------------------------------------------------
std::vector<MidiDeviceInfo> MidiInput::enumerateDevices() {
    std::vector<MidiDeviceInfo> devices;

    UINT numDevices = midiInGetNumDevs();
    for (UINT i = 0; i < numDevices; ++i) {
        MIDIINCAPSW caps;
        if (midiInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            MidiDeviceInfo info;
            info.id = static_cast<int>(i);

            // Convert wide string to UTF-8
            int len = WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, nullptr, 0, nullptr, nullptr);
            if (len > 0) {
                info.name.resize(len - 1);
                WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, &info.name[0], len, nullptr, nullptr);
            }

            devices.push_back(info);
        }
    }

    return devices;
}

//------------------------------------------------------------------------
bool MidiInput::open(int deviceId) {
    if (deviceId < 0) {
        lastError_ = "Invalid device ID";
        return false;
    }

    close();

    // Get device name
    MIDIINCAPSW caps;
    if (midiInGetDevCapsW(deviceId, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
        int len = WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, nullptr, 0, nullptr, nullptr);
        if (len > 0) {
            deviceName_.resize(len - 1);
            WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, &deviceName_[0], len, nullptr, nullptr);
        }
    }

    MMRESULT result = midiInOpen(&midiIn_, deviceId, (DWORD_PTR)midiCallback,
                                  (DWORD_PTR)this, CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) {
        lastError_ = "Failed to open MIDI device (error " + std::to_string(result) + ")";
        midiIn_ = nullptr;
        return false;
    }

    // Start receiving MIDI messages
    result = midiInStart(midiIn_);
    if (result != MMSYSERR_NOERROR) {
        midiInClose(midiIn_);
        midiIn_ = nullptr;
        lastError_ = "Failed to start MIDI input (error " + std::to_string(result) + ")";
        return false;
    }

    return true;
}

//------------------------------------------------------------------------
void MidiInput::close() {
    if (midiIn_) {
        midiInStop(midiIn_);
        midiInClose(midiIn_);
        midiIn_ = nullptr;
    }
    deviceName_.clear();
}

//------------------------------------------------------------------------
void CALLBACK MidiInput::midiCallback(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance,
                                       DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    (void)hMidiIn;
    (void)dwParam2;

    if (wMsg == MIM_DATA) {
        MidiInput* self = reinterpret_cast<MidiInput*>(dwInstance);
        if (self) {
            self->handleMidiMessage(dwParam1);
        }
    }
}

//------------------------------------------------------------------------
void MidiInput::handleMidiMessage(DWORD_PTR dwParam1) {
    // Parse MIDI message
    BYTE status = dwParam1 & 0xFF;
    BYTE data1 = (dwParam1 >> 8) & 0xFF;
    BYTE data2 = (dwParam1 >> 16) & 0xFF;

    BYTE messageType = status & 0xF0;
    BYTE channel = status & 0x0F;

    switch (messageType) {
        case 0x90:  // Note On
            if (noteCallback_) {
                if (data2 > 0) {
                    noteCallback_(channel, data1, data2);
                } else {
                    // Velocity 0 = Note Off
                    noteCallback_(channel, data1, 0);
                }
            }
            break;

        case 0x80:  // Note Off
            if (noteCallback_) {
                noteCallback_(channel, data1, 0);
            }
            break;

        case 0xA0:  // Polyphonic Aftertouch (per-note)
            if (aftertouchCallback_) {
                aftertouchCallback_(channel, data1, data2);  // note, pressure
            }
            break;

        case 0xD0:  // Channel Aftertouch (all notes)
            if (aftertouchCallback_) {
                aftertouchCallback_(channel, -1, data1);  // -1 = all notes, pressure
            }
            break;

        case 0xB0:  // Control Change
            if (ccCallback_) {
                ccCallback_(channel, data1, data2);  // controller, value
            }
            break;
    }
}

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
