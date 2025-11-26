//------------------------------------------------------------------------
// FlaschenTaschen Standalone Test Application
//
// Tests: XML parsing, FlaschenTaschen UDP client, eSpeak TTS, WASAPI audio
//
// Keyboard controls:
//   Keys A-Z map to MIDI notes 60-85 (C4-C#6)
//   Keys 0-9 map to MIDI notes 48-57 (C3-A3)
//   ESC to quit
//------------------------------------------------------------------------

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <conio.h>
#include <iostream>
#include <string>
#include <atomic>
#include <mutex>
#include <queue>

#include "WasapiAudio.h"

// Include shared sources from VST plugin
#include "../../FlaschenTaschen/source/MappingConfig.h"
#include "../../FlaschenTaschen/source/MappingConfig.cpp"
#include "../../FlaschenTaschen/source/FlaschenTaschenClient.h"
#include "../../FlaschenTaschen/source/FlaschenTaschenClient.cpp"
#include "../../FlaschenTaschen/source/BitmapFont.h"
#include "../../FlaschenTaschen/source/BitmapFont.cpp"
#include "../../FlaschenTaschen/source/ESpeakSynthesizer.h"
#include "../../FlaschenTaschen/source/ESpeakSynthesizer.cpp"
#include "../../FlaschenTaschen/source/WorldPitchShifter.h"
#include "../../FlaschenTaschen/source/WorldPitchShifter.cpp"

using namespace FlaschenTaschen;

//------------------------------------------------------------------------
// Global state
//------------------------------------------------------------------------
std::atomic<bool> g_running{true};
std::mutex g_ttsBufferMutex;
std::vector<float> g_ttsAudioBuffer;

MappingConfig g_config;
FlaschenTaschenClient g_ftClient;
BitmapFont g_font;
ESpeakSynthesizer g_tts;
WorldPitchShifter g_pitchShifter;
bool g_pitchShiftEnabled = true;  // Enable/disable pitch shifting

// Current display state
std::string g_currentSyllable;
std::mutex g_displayMutex;

//------------------------------------------------------------------------
// Convert keyboard key to MIDI note
//------------------------------------------------------------------------
int keyToMidiNote(int key) {
    // Number keys 0-9 -> MIDI 48-57 (C3-A3)
    if (key >= '0' && key <= '9') {
        return 48 + (key - '0');
    }
    // Letter keys A-Z -> MIDI 60-85 (C4-C#6)
    if (key >= 'A' && key <= 'Z') {
        return 60 + (key - 'A');
    }
    if (key >= 'a' && key <= 'z') {
        return 60 + (key - 'a');
    }
    return -1;
}

//------------------------------------------------------------------------
// Handle note trigger
//------------------------------------------------------------------------
void triggerNote(int midiNote) {
    std::string syllable = g_config.getSyllableForNote(midiNote);

    if (syllable.empty()) {
        std::cout << "  Note " << midiNote << " -> (not mapped)" << std::endl;
        return;
    }

    std::cout << "  Note " << midiNote << " -> \"" << syllable << "\"" << std::endl;

    // Update current syllable
    {
        std::lock_guard<std::mutex> lock(g_displayMutex);
        g_currentSyllable = syllable;
    }

    // Send to FlaschenTaschen display
    if (g_ftClient.isConnected()) {
        const auto& display = g_config.getDisplayConfig();
        Color textColor(display.colorR, display.colorG, display.colorB);
        Color bgColor(display.bgColorR, display.bgColorG, display.bgColorB);

        g_ftClient.clear(bgColor);
        g_font.renderTextCenteredFull(g_ftClient, syllable, textColor, bgColor);

        if (g_ftClient.send()) {
            std::cout << "    -> Sent to LED display" << std::endl;
        }
        else {
            std::cout << "    -> Failed to send: " << g_ftClient.getLastError() << std::endl;
        }
    }

    // Speak via TTS with pitch shifting
    if (g_tts.isInitialized()) {
        // Get TTS audio synchronously
        g_tts.speak(syllable);

        // Copy generated audio to playback buffer
        auto samples = g_tts.getAudioSamples();
        if (!samples.empty()) {
            // Apply pitch shifting based on MIDI note
            if (g_pitchShiftEnabled) {
                double targetFreq = WorldPitchShifter::midiNoteToFrequency(midiNote);
                samples = g_pitchShifter.processToFrequency(samples, targetFreq);
                std::cout << "    -> Pitch shifted to " << targetFreq << " Hz (MIDI " << midiNote << ")" << std::endl;
            }

            std::lock_guard<std::mutex> lock(g_ttsBufferMutex);
            g_ttsAudioBuffer.insert(g_ttsAudioBuffer.end(), samples.begin(), samples.end());
            std::cout << "    -> TTS generated " << samples.size() << " samples" << std::endl;
        }
    }
}

//------------------------------------------------------------------------
// Audio callback for WASAPI
//------------------------------------------------------------------------
void audioCallback(float* buffer, int numFrames, int numChannels) {
    std::lock_guard<std::mutex> lock(g_ttsBufferMutex);

    int samplesToWrite = numFrames * numChannels;
    int samplesAvailable = static_cast<int>(g_ttsAudioBuffer.size());

    // TTS is mono, we need to copy to all channels
    int ttsFramesToRead = (std::min)(numFrames, samplesAvailable);

    for (int frame = 0; frame < numFrames; ++frame) {
        float sample = 0.0f;

        if (frame < ttsFramesToRead && frame < static_cast<int>(g_ttsAudioBuffer.size())) {
            sample = g_ttsAudioBuffer[frame];
        }

        // Write to all channels
        for (int ch = 0; ch < numChannels; ++ch) {
            buffer[frame * numChannels + ch] = sample;
        }
    }

    // Remove consumed samples
    if (ttsFramesToRead > 0 && ttsFramesToRead <= static_cast<int>(g_ttsAudioBuffer.size())) {
        g_ttsAudioBuffer.erase(g_ttsAudioBuffer.begin(), g_ttsAudioBuffer.begin() + ttsFramesToRead);
    }
}

//------------------------------------------------------------------------
// Print usage
//------------------------------------------------------------------------
void printUsage() {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "  FlaschenTaschen Standalone Test\n";
    std::cout << "  with World Vocoder Pitch Shifting\n";
    std::cout << "========================================\n";
    std::cout << "\n";
    std::cout << "Keyboard Controls:\n";
    std::cout << "  A-Z  -> MIDI notes 60-85 (C4 - C#6)\n";
    std::cout << "  0-9  -> MIDI notes 48-57 (C3 - A3)\n";
    std::cout << "  P    -> Toggle pitch shifting ON/OFF\n";
    std::cout << "  ESC  -> Quit\n";
    std::cout << "\n";
}

//------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    std::cout << "FlaschenTaschen Standalone Test Application\n";
    std::cout << "============================================\n\n";

    // Get XML file path from argument or use default
    std::string xmlPath;
    if (argc > 1) {
        xmlPath = argv[1];
    }
    else {
        // Try default example path
        xmlPath = "../../FlaschenTaschen/examples/example_mapping.xml";
    }

    // Load XML configuration
    std::cout << "[1] Loading XML configuration: " << xmlPath << "\n";
    if (g_config.loadFromFile(xmlPath)) {
        std::cout << "    OK - Loaded " << g_config.getSyllables().size() << " syllables, "
                  << g_config.getNoteMappings().size() << " note mappings\n";
        std::cout << "    Server: " << g_config.getServerConfig().ip << ":"
                  << g_config.getServerConfig().port << "\n";
    }
    else {
        std::cout << "    FAILED: " << g_config.getLastError() << "\n";
        std::cout << "    Using built-in test configuration...\n";

        // Create a simple test config
        std::string testXml = R"(
            <Mapping>
                <Global>
                    <Server ip="127.0.0.1" port="1337"/>
                    <Display width="45" height="35" colorR="255" colorG="255" colorB="0"/>
                </Global>
                <Syllables>
                    <S id="0" text="do"/>
                    <S id="1" text="re"/>
                    <S id="2" text="mi"/>
                    <S id="3" text="fa"/>
                    <S id="4" text="sol"/>
                    <S id="5" text="la"/>
                    <S id="6" text="si"/>
                    <S id="7" text="DO"/>
                </Syllables>
                <Notes>
                    <Note midi="60" syllable_id="0"/>
                    <Note midi="62" syllable_id="1"/>
                    <Note midi="64" syllable_id="2"/>
                    <Note midi="65" syllable_id="3"/>
                    <Note midi="67" syllable_id="4"/>
                    <Note midi="69" syllable_id="5"/>
                    <Note midi="71" syllable_id="6"/>
                    <Note midi="72" syllable_id="7"/>
                </Notes>
            </Mapping>
        )";
        g_config.loadFromString(testXml);
        std::cout << "    Test config loaded: do-re-mi-fa-sol-la-si-DO on white keys\n";
    }

    // Connect to FlaschenTaschen server
    std::cout << "\n[2] Connecting to FlaschenTaschen server...\n";
    const auto& server = g_config.getServerConfig();
    const auto& display = g_config.getDisplayConfig();

    g_ftClient.setDisplaySize(display.width, display.height);
    g_ftClient.setOffset(display.offsetX, display.offsetY);
    g_ftClient.setLayer(display.layer);
    g_font.setScale(2);  // Double size for visibility

    if (g_ftClient.connect(server.ip, server.port)) {
        std::cout << "    OK - Connected to " << server.ip << ":" << server.port << "\n";

        // Send test frame
        g_ftClient.clear(Color::Black());
        g_font.renderTextCenteredFull(g_ftClient, "READY", Color::Green(), Color::Black());
        g_ftClient.send();
    }
    else {
        std::cout << "    SKIPPED - " << g_ftClient.getLastError() << "\n";
        std::cout << "    (LED display will not be updated)\n";
    }

    // Initialize WASAPI audio
    std::cout << "\n[3] Initializing WASAPI audio...\n";
    WasapiAudio audio;
    if (audio.initialize()) {
        std::cout << "    OK - " << audio.getSampleRate() << " Hz, "
                  << audio.getNumChannels() << " channels, "
                  << audio.getBufferFrames() << " buffer frames\n";
    }
    else {
        std::cout << "    FAILED: " << audio.getLastError() << "\n";
        std::cout << "    (Audio will not play)\n";
    }

    // Initialize eSpeak TTS
    std::cout << "\n[4] Initializing eSpeak-NG TTS...\n";
    if (g_tts.initialize(audio.getSampleRate())) {
        const auto& tts = g_config.getTTSConfig();
        std::cout << "    OK - TTS initialized at " << audio.getSampleRate() << " Hz\n";
        g_tts.setVoice(tts.voice);
        g_tts.setRate(tts.rate);
        g_tts.setPitch(tts.pitch);
        g_tts.setVolume(tts.volume);
        std::cout << "    Voice: " << tts.voice << ", Rate: " << tts.rate
                  << ", Pitch: " << tts.pitch << ", Volume: " << tts.volume << "\n";
    }
    else {
        std::cout << "    SKIPPED - " << g_tts.getLastError() << "\n";
        std::cout << "    (Speech synthesis disabled)\n";
    }

    // Initialize World vocoder for pitch shifting
    std::cout << "\n[5] Initializing World vocoder...\n";
    g_pitchShifter.initialize(audio.getSampleRate());
    std::cout << "    OK - Pitch shifter ready at " << audio.getSampleRate() << " Hz\n";
    std::cout << "    Press 'P' to toggle pitch shifting (currently ON)\n";

    // Start audio
    std::cout << "\n[6] Starting audio playback...\n";
    if (audio.isRunning() || audio.start(audioCallback)) {
        std::cout << "    OK - Audio running\n";
    }
    else {
        std::cout << "    FAILED: " << audio.getLastError() << "\n";
    }

    // Print usage
    printUsage();

    // Show mapped notes
    std::cout << "Mapped Notes:\n";
    for (const auto& nm : g_config.getNoteMappings()) {
        const auto* syl = g_config.getSyllableById(nm.syllableId);
        if (syl) {
            std::cout << "  MIDI " << nm.midiNote << " -> \"" << syl->text << "\"\n";
        }
    }
    std::cout << "\nPress keys to trigger notes (ESC to quit):\n\n";

    // Main loop - keyboard input
    while (g_running) {
        if (_kbhit()) {
            int key = _getch();

            // Check for escape
            if (key == 27) {  // ESC
                g_running = false;
                break;
            }

            // Check for special keys (arrows, function keys start with 0 or 224)
            if (key == 0 || key == 224) {
                _getch();  // Consume the second byte
                continue;
            }

            // Toggle pitch shifting with 'P'
            if (key == 'p' || key == 'P') {
                g_pitchShiftEnabled = !g_pitchShiftEnabled;
                std::cout << "  Pitch shifting: " << (g_pitchShiftEnabled ? "ON" : "OFF") << std::endl;
                continue;
            }

            // Convert key to MIDI note
            int midiNote = keyToMidiNote(key);
            if (midiNote >= 0) {
                triggerNote(midiNote);
            }
        }

        Sleep(10);  // Small delay to prevent CPU spinning
    }

    // Cleanup
    std::cout << "\nShutting down...\n";

    audio.stop();
    g_tts.shutdown();
    g_ftClient.disconnect();

    std::cout << "Done.\n";
    return 0;
}
