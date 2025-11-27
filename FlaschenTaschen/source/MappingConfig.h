//------------------------------------------------------------------------
// Copyright(c) 2024 Stratojets.
//------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace FlaschenTaschen {

//------------------------------------------------------------------------
// Syllable - represents a single syllable entry
//------------------------------------------------------------------------
struct Syllable {
    int id;
    std::string text;
};

//------------------------------------------------------------------------
// NoteMapping - maps a MIDI note to a syllable
//------------------------------------------------------------------------
struct NoteMapping {
    int midiNote;
    int syllableId;
};

//------------------------------------------------------------------------
// ServerConfig - FlaschenTaschen server configuration
//------------------------------------------------------------------------
struct ServerConfig {
    std::string ip = "127.0.0.1";
    int port = 1337;
};

//------------------------------------------------------------------------
// DisplayConfig - LED matrix display configuration
//------------------------------------------------------------------------
struct DisplayConfig {
    int width = 45;
    int height = 35;
    int offsetX = 0;
    int offsetY = 0;
    int layer = 1;  // Z-layer (0 = background)
    bool flipHorizontal = false;  // Flip entire display horizontally
    bool mirrorGlyph = true;      // Mirror each character/glyph horizontally

    // Font/color settings
    uint8_t colorR = 255;
    uint8_t colorG = 255;
    uint8_t colorB = 255;
    uint8_t bgColorR = 0;
    uint8_t bgColorG = 0;
    uint8_t bgColorB = 0;
};

//------------------------------------------------------------------------
// TTSConfig - Text-to-speech configuration
//------------------------------------------------------------------------
struct TTSConfig {
    std::string voice = "en";   // Voice name (e.g., "en", "de", "fr")
    int rate = 120;             // Words per minute (80-450)
    int pitch = 50;             // Pitch (0-99)
    int volume = 100;           // Volume (0-200)
};

//------------------------------------------------------------------------
// AudioConfig - Audio device configuration
//------------------------------------------------------------------------
struct AudioConfig {
    std::string deviceId;       // WASAPI device ID (empty = default)
    std::string deviceName;     // Friendly name (for display only)
    int bufferMs = 20;          // Buffer size in milliseconds (default 20ms)
};

//------------------------------------------------------------------------
// MidiConfig - MIDI input configuration
//------------------------------------------------------------------------
struct MidiConfig {
    int deviceId = -1;          // MIDI device ID (-1 = disabled)
    std::string deviceName;     // Device name (for display only)
};

//------------------------------------------------------------------------
// EffectType - types of visual effects
//------------------------------------------------------------------------
enum class EffectType {
    None = 0,
    SolidColor,     // Fill with solid color
    ColorRamp,      // Gradient from color1 to color2
    Pulse,          // Pulsing color (fades in/out)
    Rainbow,        // Cycling rainbow colors
    Flash,          // Quick flash then fade
    Strobe,         // Rapid on/off
    Wave,           // Horizontal wave pattern
    Sparkle         // Random sparkling pixels
};

//------------------------------------------------------------------------
// RampDirection - direction for color ramps
//------------------------------------------------------------------------
enum class RampDirection {
    Horizontal = 0,
    Vertical,
    DiagonalDown,
    DiagonalUp,
    Radial          // From center outward
};

//------------------------------------------------------------------------
// Effect - defines a visual effect with parameters
//------------------------------------------------------------------------
struct Effect {
    int id = -1;
    std::string name;
    EffectType type = EffectType::None;

    // Colors
    uint8_t color1R = 255, color1G = 255, color1B = 255;  // Primary color
    uint8_t color2R = 0, color2G = 0, color2B = 0;        // Secondary color (for ramps, etc.)

    // Timing parameters (in milliseconds)
    int durationMs = 500;       // Total effect duration
    int periodMs = 100;         // Period for repeating effects (pulse, strobe)

    // Ramp parameters
    RampDirection rampDirection = RampDirection::Horizontal;

    // Other parameters
    float intensity = 1.0f;     // 0.0 - 1.0
    int speed = 50;             // Effect speed (0-100)

    // Helper to convert effect type from string
    static EffectType typeFromString(const std::string& str);
    static RampDirection directionFromString(const std::string& str);
};

//------------------------------------------------------------------------
// EffectMapping - maps a MIDI note to an effect
//------------------------------------------------------------------------
struct EffectMapping {
    int midiNote;
    int effectId;
};

//------------------------------------------------------------------------
// MappingConfig - complete configuration from XML
//------------------------------------------------------------------------
class MappingConfig {
public:
    MappingConfig() = default;
    ~MappingConfig() = default;

    // Load configuration from XML file
    bool loadFromFile(const std::string& filePath);

    // Load configuration from XML string
    bool loadFromString(const std::string& xmlContent);

    // Getters
    const ServerConfig& getServerConfig() const { return serverConfig_; }
    const DisplayConfig& getDisplayConfig() const { return displayConfig_; }
    const TTSConfig& getTTSConfig() const { return ttsConfig_; }
    const AudioConfig& getAudioConfig() const { return audioConfig_; }
    const MidiConfig& getMidiConfig() const { return midiConfig_; }
    const std::vector<Syllable>& getSyllables() const { return syllables_; }
    const std::vector<NoteMapping>& getNoteMappings() const { return noteMappings_; }
    const std::vector<Effect>& getEffects() const { return effects_; }
    const std::vector<EffectMapping>& getEffectMappings() const { return effectMappings_; }

    // Get syllable text for a given MIDI note (returns empty string if not found)
    std::string getSyllableForNote(int midiNote) const;

    // Get syllable by ID
    const Syllable* getSyllableById(int id) const;

    // Get effect for a given MIDI note (returns nullptr if not found)
    const Effect* getEffectForNote(int midiNote) const;

    // Get effect by ID
    const Effect* getEffectById(int id) const;

    // Check if a note triggers an effect (vs syllable)
    bool hasEffectForNote(int midiNote) const;

    // Check if configuration is valid
    bool isValid() const { return isValid_; }

    // Get last error message
    const std::string& getLastError() const { return lastError_; }

    // Setters for programmatic configuration
    void setServerConfig(const ServerConfig& config) { serverConfig_ = config; }
    void setDisplayConfig(const DisplayConfig& config) { displayConfig_ = config; }

private:
    ServerConfig serverConfig_;
    DisplayConfig displayConfig_;
    TTSConfig ttsConfig_;
    AudioConfig audioConfig_;
    MidiConfig midiConfig_;
    std::vector<Syllable> syllables_;
    std::vector<NoteMapping> noteMappings_;
    std::vector<Effect> effects_;
    std::vector<EffectMapping> effectMappings_;
    std::map<int, int> noteToSyllableMap_;  // MIDI note -> syllable ID
    std::map<int, int> noteToEffectMap_;    // MIDI note -> effect ID

    bool isValid_ = false;
    std::string lastError_;

    void buildNoteToSyllableMap();
    void buildNoteToEffectMap();
    bool parseXml(const std::string& xmlContent);
};

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
