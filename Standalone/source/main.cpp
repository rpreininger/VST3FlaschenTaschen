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
#include <cmath>

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
int g_octaveOffset = 0;           // Octave shift (-3 to +3)

// Sample rate conversion
int g_ttsSampleRate = 22050;  // eSpeak default
int g_outputSampleRate = 48000;  // Will be set from WASAPI

// Simple linear resampler
std::vector<float> resample(const std::vector<float>& input, int inputRate, int outputRate) {
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

// Current display state
std::string g_currentSyllable;
std::mutex g_displayMutex;

//------------------------------------------------------------------------
// Convert keyboard key to MIDI note (base note for syllable lookup)
// Home row (A,S,D,F,G,H,J,K) = C major scale C2-C3
//------------------------------------------------------------------------
int keyToMidiNote(int key) {
    // Convert to uppercase for easier handling
    if (key >= 'a' && key <= 'z') {
        key = key - 'a' + 'A';
    }

    // Home row: C major scale C2-C3
    switch (key) {
        case 'A': return 36;  // C2
        case 'S': return 38;  // D2
        case 'D': return 40;  // E2
        case 'F': return 41;  // F2
        case 'G': return 43;  // G2
        case 'H': return 45;  // A2
        case 'J': return 47;  // B2 (H2 in German)
        case 'K': return 48;  // C3
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
            // Apply pitch shifting based on MIDI note + octave offset
            if (g_pitchShiftEnabled) {
                int pitchNote = midiNote + (g_octaveOffset * 12);
                if (pitchNote < 0) pitchNote = 0;
                if (pitchNote > 127) pitchNote = 127;
                double targetFreq = WorldPitchShifter::midiNoteToFrequency(pitchNote);
                samples = g_pitchShifter.processToFrequency(samples, targetFreq);
                std::cout << "    -> Pitch shifted to " << targetFreq << " Hz (MIDI " << pitchNote << ")" << std::endl;
            }

            // Resample from TTS rate to output rate
            if (g_ttsSampleRate != g_outputSampleRate) {
                samples = resample(samples, g_ttsSampleRate, g_outputSampleRate);
                std::cout << "    -> Resampled " << g_ttsSampleRate << " -> " << g_outputSampleRate << " Hz" << std::endl;
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
    std::cout << "Keyboard Controls (Home Row = C Major Scale):\n";
    std::cout << "  A=C  S=D  D=E  F=F  G=G  H=A  J=B  K=C (default: C2-C3)\n";
    std::cout << "\n";
    std::cout << "  W/+  -> Octave UP\n";
    std::cout << "  Q/-  -> Octave DOWN\n";
    std::cout << "  P    -> Toggle pitch shifting ON/OFF\n";
    std::cout << "  T    -> Test tone (440 Hz)\n";
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

        // Create default config: C major scale C2-C3 on home row keys
        std::string testXml = R"(
            <Mapping>
                <Global>
                    <Server ip="127.0.0.1" port="1337"/>
                    <Display width="45" height="35" colorR="255" colorG="255" colorB="0"/>
                    <TTS voice="en" rate="175" pitch="50" volume="100"/>
                </Global>
                <Syllables>
                    <S id="0" text="the"/>
                    <S id="1" text="strato"/>
                    <S id="2" text="jets"/>
                    <S id="3" text="are"/>
                    <S id="4" text="the"/>
                    <S id="5" text="next"/>
                    <S id="6" text="hot"/>
                    <S id="7" text="shit"/>
                </Syllables>
                <Notes>
                    <Note midi="36" syllable_id="0"/>
                    <Note midi="38" syllable_id="1"/>
                    <Note midi="40" syllable_id="2"/>
                    <Note midi="41" syllable_id="3"/>
                    <Note midi="43" syllable_id="4"/>
                    <Note midi="45" syllable_id="5"/>
                    <Note midi="47" syllable_id="6"/>
                    <Note midi="48" syllable_id="7"/>
                </Notes>
            </Mapping>
        )";
        g_config.loadFromString(testXml);
        std::cout << "    Default: the strato jets are the next hot shit\n";
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
        g_outputSampleRate = audio.getSampleRate();
        std::cout << "    OK - " << g_outputSampleRate << " Hz, "
                  << audio.getNumChannels() << " channels, "
                  << audio.getBufferFrames() << " buffer frames\n";
    }
    else {
        std::cout << "    FAILED: " << audio.getLastError() << "\n";
        std::cout << "    (Audio will not play)\n";
    }

    // Initialize eSpeak TTS (at 22050 Hz - eSpeak's native rate)
    std::cout << "\n[4] Initializing eSpeak-NG TTS...\n";
    g_ttsSampleRate = 22050;  // eSpeak's default/preferred rate
    if (g_tts.initialize(g_ttsSampleRate)) {
        const auto& tts = g_config.getTTSConfig();
        g_ttsSampleRate = g_tts.getSampleRate();  // Get actual rate from eSpeak
        std::cout << "    OK - TTS initialized at " << g_ttsSampleRate << " Hz\n";
        std::cout << "    Output rate: " << g_outputSampleRate << " Hz (will resample)\n";
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

    // Initialize World vocoder for pitch shifting (at TTS sample rate)
    std::cout << "\n[5] Initializing World vocoder...\n";
    g_pitchShifter.initialize(g_ttsSampleRate);
    std::cout << "    OK - Pitch shifter ready at " << g_ttsSampleRate << " Hz\n";
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

            // Octave up with 'W' or '+'
            if (key == 'w' || key == 'W' || key == '+') {
                if (g_octaveOffset < 5) {
                    g_octaveOffset++;
                    int baseC = 36 + (g_octaveOffset * 12);  // C note at current octave
                    int octaveNum = (baseC / 12) - 1;  // MIDI octave numbering
                    std::cout << "  Octave UP -> C" << octaveNum << "-C" << (octaveNum + 1)
                              << " (offset " << (g_octaveOffset >= 0 ? "+" : "") << g_octaveOffset << ")" << std::endl;
                } else {
                    std::cout << "  Octave: already at maximum" << std::endl;
                }
                continue;
            }

            // Octave down with 'Q' or '-'
            if (key == 'q' || key == 'Q' || key == '-') {
                if (g_octaveOffset > -3) {
                    g_octaveOffset--;
                    int baseC = 36 + (g_octaveOffset * 12);  // C note at current octave
                    int octaveNum = (baseC / 12) - 1;  // MIDI octave numbering
                    std::cout << "  Octave DOWN -> C" << octaveNum << "-C" << (octaveNum + 1)
                              << " (offset " << (g_octaveOffset >= 0 ? "+" : "") << g_octaveOffset << ")" << std::endl;
                } else {
                    std::cout << "  Octave: already at minimum" << std::endl;
                }
                continue;
            }

            // Test tone with 'T'
            if (key == 't' || key == 'T') {
                std::cout << "  Playing test tone (440 Hz)..." << std::endl;
                std::vector<float> testTone(g_outputSampleRate);  // 1 second
                for (int i = 0; i < g_outputSampleRate; ++i) {
                    testTone[i] = 0.3f * std::sin(2.0f * 3.14159f * 440.0f * i / g_outputSampleRate);
                }
                {
                    std::lock_guard<std::mutex> lock(g_ttsBufferMutex);
                    g_ttsAudioBuffer.insert(g_ttsAudioBuffer.end(), testTone.begin(), testTone.end());
                }
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
