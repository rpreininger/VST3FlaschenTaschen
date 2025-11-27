//------------------------------------------------------------------------
// Copyright(c) 2024 Stratojets.
//------------------------------------------------------------------------

#include "MappingConfig.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace FlaschenTaschen {

//------------------------------------------------------------------------
// Simple XML Parser Helper Functions
//------------------------------------------------------------------------
namespace {

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

std::string getAttribute(const std::string& tag, const std::string& attrName) {
    std::string searchStr = attrName + "=\"";
    size_t pos = tag.find(searchStr);
    if (pos == std::string::npos) {
        // Try single quotes
        searchStr = attrName + "='";
        pos = tag.find(searchStr);
        if (pos == std::string::npos) return "";
    }

    pos += searchStr.length();
    char quote = tag[pos - 1];
    size_t endPos = tag.find(quote, pos);
    if (endPos == std::string::npos) return "";

    return tag.substr(pos, endPos - pos);
}

int getIntAttribute(const std::string& tag, const std::string& attrName, int defaultValue = 0) {
    std::string val = getAttribute(tag, attrName);
    if (val.empty()) return defaultValue;
    try {
        return std::stoi(val);
    } catch (...) {
        return defaultValue;
    }
}

uint8_t getUint8Attribute(const std::string& tag, const std::string& attrName, uint8_t defaultValue = 0) {
    return static_cast<uint8_t>(getIntAttribute(tag, attrName, defaultValue));
}

std::vector<std::string> findAllTags(const std::string& xml, const std::string& tagName) {
    std::vector<std::string> results;
    std::string openTag = "<" + tagName;
    size_t pos = 0;

    while ((pos = xml.find(openTag, pos)) != std::string::npos) {
        size_t endPos = xml.find(">", pos);
        if (endPos == std::string::npos) break;

        // Include the full tag (handle self-closing tags)
        std::string tag = xml.substr(pos, endPos - pos + 1);
        results.push_back(tag);
        pos = endPos + 1;
    }

    return results;
}

std::string findTagContent(const std::string& xml, const std::string& tagName) {
    std::string openTag = "<" + tagName;
    std::string closeTag = "</" + tagName + ">";

    size_t startPos = xml.find(openTag);
    if (startPos == std::string::npos) return "";

    size_t contentStart = xml.find(">", startPos);
    if (contentStart == std::string::npos) return "";
    contentStart++;

    size_t endPos = xml.find(closeTag, contentStart);
    if (endPos == std::string::npos) return "";

    return xml.substr(contentStart, endPos - contentStart);
}

} // anonymous namespace

//------------------------------------------------------------------------
// MappingConfig Implementation
//------------------------------------------------------------------------
bool MappingConfig::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        lastError_ = "Failed to open file: " + filePath;
        isValid_ = false;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return loadFromString(buffer.str());
}

bool MappingConfig::loadFromString(const std::string& xmlContent) {
    syllables_.clear();
    noteMappings_.clear();
    noteToSyllableMap_.clear();
    isValid_ = false;

    if (!parseXml(xmlContent)) {
        return false;
    }

    buildNoteToSyllableMap();
    isValid_ = true;
    return true;
}

bool MappingConfig::parseXml(const std::string& xmlContent) {
    // Parse Global/Server section
    std::string globalSection = findTagContent(xmlContent, "Global");
    if (!globalSection.empty()) {
        auto serverTags = findAllTags(globalSection, "Server");
        if (!serverTags.empty()) {
            serverConfig_.ip = getAttribute(serverTags[0], "ip");
            serverConfig_.port = getIntAttribute(serverTags[0], "port", 1337);
        }

        // Parse Display config if present
        auto displayTags = findAllTags(globalSection, "Display");
        if (!displayTags.empty()) {
            displayConfig_.width = getIntAttribute(displayTags[0], "width", 45);
            displayConfig_.height = getIntAttribute(displayTags[0], "height", 35);
            displayConfig_.offsetX = getIntAttribute(displayTags[0], "offsetX", 0);
            displayConfig_.offsetY = getIntAttribute(displayTags[0], "offsetY", 0);
            displayConfig_.layer = getIntAttribute(displayTags[0], "layer", 1);
            // Parse flipHorizontal - default false, "1" or "true" enables it
            std::string flipStr = getAttribute(displayTags[0], "flipHorizontal");
            if (!flipStr.empty()) {
                displayConfig_.flipHorizontal = (flipStr == "1" || flipStr == "true");
            }
            // Parse mirrorGlyph - default true, "0" or "false" disables it
            std::string mirrorStr = getAttribute(displayTags[0], "mirrorGlyph");
            if (!mirrorStr.empty()) {
                displayConfig_.mirrorGlyph = (mirrorStr != "0" && mirrorStr != "false");
            }
            displayConfig_.colorR = getUint8Attribute(displayTags[0], "colorR", 255);
            displayConfig_.colorG = getUint8Attribute(displayTags[0], "colorG", 255);
            displayConfig_.colorB = getUint8Attribute(displayTags[0], "colorB", 255);
            displayConfig_.bgColorR = getUint8Attribute(displayTags[0], "bgColorR", 0);
            displayConfig_.bgColorG = getUint8Attribute(displayTags[0], "bgColorG", 0);
            displayConfig_.bgColorB = getUint8Attribute(displayTags[0], "bgColorB", 0);
        }

        // Parse TTS config if present
        auto ttsTags = findAllTags(globalSection, "TTS");
        if (!ttsTags.empty()) {
            std::string voice = getAttribute(ttsTags[0], "voice");
            if (!voice.empty()) {
                ttsConfig_.voice = voice;
            }
            ttsConfig_.rate = getIntAttribute(ttsTags[0], "rate", 120);
            ttsConfig_.pitch = getIntAttribute(ttsTags[0], "pitch", 50);
            ttsConfig_.volume = getIntAttribute(ttsTags[0], "volume", 100);
        }

        // Parse Audio config if present
        auto audioTags = findAllTags(globalSection, "Audio");
        if (!audioTags.empty()) {
            audioConfig_.deviceId = getAttribute(audioTags[0], "deviceId");
            audioConfig_.deviceName = getAttribute(audioTags[0], "deviceName");
            audioConfig_.bufferMs = getIntAttribute(audioTags[0], "bufferMs", 20);
        }

        // Parse MIDI config if present
        auto midiTags = findAllTags(globalSection, "Midi");
        if (!midiTags.empty()) {
            midiConfig_.deviceId = getIntAttribute(midiTags[0], "deviceId", -1);
            midiConfig_.deviceName = getAttribute(midiTags[0], "deviceName");
        }
    }

    // Parse Syllables section
    std::string syllablesSection = findTagContent(xmlContent, "Syllables");
    if (!syllablesSection.empty()) {
        auto syllableTags = findAllTags(syllablesSection, "S");
        for (const auto& tag : syllableTags) {
            Syllable s;
            s.id = getIntAttribute(tag, "id", -1);
            s.text = getAttribute(tag, "text");

            if (s.id >= 0 && !s.text.empty()) {
                syllables_.push_back(s);
            }
        }
    }

    // Parse Notes section
    std::string notesSection = findTagContent(xmlContent, "Notes");
    if (!notesSection.empty()) {
        auto noteTags = findAllTags(notesSection, "Note");
        for (const auto& tag : noteTags) {
            NoteMapping nm;
            nm.midiNote = getIntAttribute(tag, "midi", -1);
            nm.syllableId = getIntAttribute(tag, "syllable_id", -1);

            if (nm.midiNote >= 0 && nm.syllableId >= 0) {
                noteMappings_.push_back(nm);
            }
        }
    }

    if (syllables_.empty()) {
        lastError_ = "No syllables found in configuration";
        return false;
    }

    if (noteMappings_.empty()) {
        lastError_ = "No note mappings found in configuration";
        return false;
    }

    return true;
}

void MappingConfig::buildNoteToSyllableMap() {
    noteToSyllableMap_.clear();
    for (const auto& nm : noteMappings_) {
        noteToSyllableMap_[nm.midiNote] = nm.syllableId;
    }
}

std::string MappingConfig::getSyllableForNote(int midiNote) const {
    auto it = noteToSyllableMap_.find(midiNote);
    if (it == noteToSyllableMap_.end()) {
        return "";
    }

    const Syllable* s = getSyllableById(it->second);
    return s ? s->text : "";
}

const Syllable* MappingConfig::getSyllableById(int id) const {
    for (const auto& s : syllables_) {
        if (s.id == id) {
            return &s;
        }
    }
    return nullptr;
}

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
