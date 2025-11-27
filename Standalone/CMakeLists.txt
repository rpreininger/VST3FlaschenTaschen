cmake_minimum_required(VERSION 3.14.0)

project(FlaschenTaschenStandalone
    VERSION 1.0.0
    DESCRIPTION "FlaschenTaschen Standalone Test Application with World Vocoder"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# World vocoder source files
set(WORLD_SOURCES
    deps/world/src/cheaptrick.cpp
    deps/world/src/common.cpp
    deps/world/src/d4c.cpp
    deps/world/src/dio.cpp
    deps/world/src/fft.cpp
    deps/world/src/harvest.cpp
    deps/world/src/matlabfunctions.cpp
    deps/world/src/stonemask.cpp
    deps/world/src/synthesis.cpp
    deps/world/src/synthesisrealtime.cpp
)

# Source files
set(SOURCES
    source/main.cpp
    source/WasapiAudio.h
    source/WasapiAudio.cpp
    source/MidiInput.h
    source/MidiInput.cpp
    ${WORLD_SOURCES}
)

# Create executable
add_executable(FlaschenTaschenTest ${SOURCES})

# Include directories
target_include_directories(FlaschenTaschenTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${CMAKE_CURRENT_SOURCE_DIR}/../FlaschenTaschen/source
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/world/src
)

# Windows-specific settings
if(WIN32)
    # Link Windows libraries
    target_link_libraries(FlaschenTaschenTest PRIVATE
        ws2_32       # Winsock for UDP
        ole32        # COM for WASAPI
        winmm        # Windows multimedia
    )

    # Disable min/max macros
    target_compile_definitions(FlaschenTaschenTest PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )

    # Console application
    set_target_properties(FlaschenTaschenTest PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
endif()

# eSpeak-NG Support - uses dynamic loading (LoadLibrary)
# Will work if eSpeak-NG is installed at runtime
message(STATUS "eSpeak-NG: using dynamic loading (will work if installed at runtime)")

# World Vocoder
message(STATUS "World Vocoder: enabled for pitch shifting")

# Copy example XML to build directory
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../FlaschenTaschen/examples/example_mapping.xml
    ${CMAKE_CURRENT_BINARY_DIR}/example_mapping.xml
    COPYONLY
)

message(STATUS "FlaschenTaschen Standalone configured")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  World Vocoder: enabled")
