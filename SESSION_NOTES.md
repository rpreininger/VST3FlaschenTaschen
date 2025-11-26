# FlaschenTaschen VST3 Plugin - Development Session Notes

## Project Overview

A VST3 plugin that maps MIDI notes to syllables, displays them on a FlaschenTaschen LED matrix via UDP, and speaks them using eSpeak-NG text-to-speech.

## Architecture

```
FlaschenTaschen/
├── FlaschenTaschen/          # VST3 Plugin
│   ├── source/
│   │   ├── MappingConfig.*      # XML parser for MIDI-syllable mappings
│   │   ├── FlaschenTaschenClient.*  # UDP client for LED matrix
│   │   ├── BitmapFont.*         # 5x7 pixel font renderer
│   │   ├── ESpeakSynthesizer.*  # eSpeak-NG TTS (dynamic loading)
│   │   ├── mypluginprocessor.*  # VST3 audio/MIDI processor
│   │   └── myplugincontroller.* # VST3 UI controller
│   ├── examples/
│   │   └── example_mapping.xml  # Sample configuration
│   └── CMakeLists.txt
├── Standalone/               # Test Application
│   ├── source/
│   │   ├── main.cpp            # Console app with keyboard input
│   │   └── WasapiAudio.*       # WASAPI audio output
│   ├── deps/espeak-ng/
│   │   └── speak_lib.h         # eSpeak-NG header (local copy)
│   └── CMakeLists.txt
└── .gitignore
```

## Key Components

### 1. MappingConfig
Parses XML configuration files with:
- `<Server>` - FlaschenTaschen server IP and port
- `<Display>` - LED matrix dimensions and colors
- `<TTS>` - Text-to-speech settings (voice, rate, pitch, volume)
- `<Syllables>` - List of syllable texts with IDs
- `<Notes>` - MIDI note to syllable ID mappings

### 2. FlaschenTaschenClient
UDP client implementing the FlaschenTaschen protocol:
- PPM P6 binary format
- Header: `#FT: X Y Z` (offset and layer)
- Port 1337 (default)

### 3. BitmapFont
5x7 pixel bitmap font renderer:
- Supports A-Z, 0-9, basic punctuation
- Centered text rendering
- Configurable scale (1x, 2x, etc.)

### 4. ESpeakSynthesizer
eSpeak-NG text-to-speech wrapper:
- **Dynamic DLL loading** (LoadLibrary/GetProcAddress)
- No link-time dependency on eSpeak-NG
- Works if eSpeak-NG is installed at runtime
- Configurable voice, rate, pitch, volume

## XML Configuration Format

```xml
<Mapping>
    <Global>
        <Server ip="127.0.0.1" port="1337"/>
        <Display width="45" height="35"
                 colorR="255" colorG="255" colorB="0"
                 bgColorR="0" bgColorG="0" bgColorB="0"/>
        <TTS voice="en+robosoft" rate="120" pitch="50" volume="100"/>
    </Global>
    <Syllables>
        <S id="0" text="do"/>
        <S id="1" text="re"/>
        <!-- ... -->
    </Syllables>
    <Notes>
        <Note midi="60" syllable_id="0"/>
        <Note midi="62" syllable_id="1"/>
        <!-- ... -->
    </Notes>
</Mapping>
```

## eSpeak-NG Voice Options

### Voice Format
Use `language+variant` syntax, e.g., `en+whisper`

### Available Languages
- `en` - English
- `de` - German
- `fr` - French
- `es` - Spanish
- `it` - Italian

### Voice Variants
| Category | Examples |
|----------|----------|
| Male | adam, Alex, Andy, david, max, mike, rob |
| Female | Alicia, Andrea, Annie, linda, steph |
| Robot | robosoft, robosoft2-8, UniRobot, anikaRobot |
| Whisper | whisper, whisperf |
| Generic | f1-f5 (female), m1-m8 (male) |

### TTS Parameters
- **rate**: Words per minute (80-450, default 120)
- **pitch**: Base pitch (0-99, default 50)
- **volume**: Volume level (0-200, default 100)

## Build Instructions

### Prerequisites
- Visual Studio 2022 with C++ workload
- CMake 3.14+
- VST3 SDK (fetched automatically)
- eSpeak-NG (optional, for TTS - install from https://github.com/espeak-ng/espeak-ng/releases)

### Build VST3 Plugin
```bash
cd FlaschenTaschen/FlaschenTaschen
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
Output: `build/VST3/Release/FlaschenTaschen.vst3`

### Build Standalone Test App
```bash
cd FlaschenTaschen/Standalone
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
Output: `build/Release/FlaschenTaschenTest.exe`

### Run Standalone
```bash
FlaschenTaschenTest.exe keyboard_mapping.xml
```
- Keys A-Z → MIDI notes 60-85
- Keys 0-9 → MIDI notes 48-57
- ESC to quit

## Known Issues / TODO

1. **VST3 Plugin UI**: Basic parameter controls only, no custom VSTGUI editor yet
2. **Sample Rate Mismatch**: eSpeak generates at 22050 Hz, WASAPI may use 48000 Hz - resampling not implemented
3. **Async TTS**: Speech synthesis is synchronous, may cause audio glitches with rapid triggers
4. **Linux/macOS**: Only Windows fully tested

## Dependencies

- **VST3 SDK**: Fetched via CMake FetchContent
- **eSpeak-NG**: Optional runtime dependency (dynamic loading)
- **Windows APIs**: Winsock2 (UDP), WASAPI (audio), COM

## Repository

https://github.com/rpreininger/VST3FlaschenTaschen
