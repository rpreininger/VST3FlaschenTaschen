//------------------------------------------------------------------------
// FlaschenTaschen Standalone - Windows MIDI Input
//------------------------------------------------------------------------

#pragma once

#include <windows.h>
#include <mmsystem.h>
#include <functional>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>

namespace FlaschenTaschen {

//------------------------------------------------------------------------
// MidiDeviceInfo - Information about a MIDI device
//------------------------------------------------------------------------
struct MidiDeviceInfo {
    int id;              // Device ID (for selection)
    std::string name;    // Device name
};

//------------------------------------------------------------------------
// MidiInput - Windows MIDI input handler
//------------------------------------------------------------------------
class MidiInput {
public:
    // MIDI callback: receives channel, note, velocity (velocity 0 = note off)
    using NoteCallback = std::function<void(int channel, int note, int velocity)>;

    // Aftertouch callback: receives channel, note (for poly) or -1 (for channel), pressure
    using AftertouchCallback = std::function<void(int channel, int note, int pressure)>;

    // Control change callback: receives channel, controller number, value
    using ControlChangeCallback = std::function<void(int channel, int controller, int value)>;

    MidiInput();
    ~MidiInput();

    // Enumerate available MIDI input devices
    static std::vector<MidiDeviceInfo> enumerateDevices();

    // Open MIDI input device by ID (-1 = no device)
    bool open(int deviceId);

    // Close MIDI input
    void close();

    // Check if open
    bool isOpen() const { return midiIn_ != nullptr; }

    // Set callbacks
    void setNoteCallback(NoteCallback callback) { noteCallback_ = callback; }
    void setAftertouchCallback(AftertouchCallback callback) { aftertouchCallback_ = callback; }
    void setControlChangeCallback(ControlChangeCallback callback) { ccCallback_ = callback; }

    // Get last error
    const std::string& getLastError() const { return lastError_; }

    // Get current device name
    const std::string& getDeviceName() const { return deviceName_; }

private:
    static void CALLBACK midiCallback(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance,
                                       DWORD_PTR dwParam1, DWORD_PTR dwParam2);
    void handleMidiMessage(DWORD_PTR dwParam1);

    HMIDIIN midiIn_ = nullptr;
    NoteCallback noteCallback_;
    AftertouchCallback aftertouchCallback_;
    ControlChangeCallback ccCallback_;
    std::string lastError_;
    std::string deviceName_;
};

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
