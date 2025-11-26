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
    const std::vector<Syllable>& getSyllables() const { return syllables_; }
    const std::vector<NoteMapping>& getNoteMappings() const { return noteMappings_; }

    // Get syllable text for a given MIDI note (returns empty string if not found)
    std::string getSyllableForNote(int midiNote) const;

    // Get syllable by ID
    const Syllable* getSyllableById(int id) const;

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
    std::vector<Syllable> syllables_;
    std::vector<NoteMapping> noteMappings_;
    std::map<int, int> noteToSyllableMap_;  // MIDI note -> syllable ID

    bool isValid_ = false;
    std::string lastError_;

    void buildNoteToSyllableMap();
    bool parseXml(const std::string& xmlContent);
};

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
