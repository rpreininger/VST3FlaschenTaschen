cmake_minimum_required(VERSION 3.14.0)

option(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES "Enable VST 3 Plug-in Examples" OFF)
option(SMTG_ENABLE_VST3_HOSTING_EXAMPLES "Enable VST 3 Hosting Examples" OFF)
option(SMTG_ENABLE_ESPEAK_NG "Enable eSpeak-NG Text-to-Speech support" OFF)

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.13 CACHE STRING "")

set(vst3sdk_SOURCE_DIR "D:/Developer/C++/VST_SDK/vst3sdk")
if(NOT vst3sdk_SOURCE_DIR)
    message(FATAL_ERROR "Path to VST3 SDK is empty!")
endif()

project(FlaschenTaschen
    VERSION 1.0.0.0
    DESCRIPTION "FlaschenTaschen VST 3 Plug-in - MIDI to LED Matrix with TTS"
)

set(SMTG_VSTGUI_ROOT "${vst3sdk_SOURCE_DIR}")

add_subdirectory(${vst3sdk_SOURCE_DIR} ${PROJECT_BINARY_DIR}/vst3sdk)
smtg_enable_vst3_sdk()

# FlaschenTaschen plugin sources
set(PLUGIN_SOURCES
    source/version.h
    source/myplugincids.h
    source/mypluginprocessor.h
    source/mypluginprocessor.cpp
    source/myplugincontroller.h
    source/myplugincontroller.cpp
    source/mypluginentry.cpp
    source/MappingConfig.h
    source/MappingConfig.cpp
    source/FlaschenTaschenClient.h
    source/FlaschenTaschenClient.cpp
    source/BitmapFont.h
    source/BitmapFont.cpp
    source/ESpeakSynthesizer.h
    source/ESpeakSynthesizer.cpp
)

smtg_add_vst3plugin(FlaschenTaschen ${PLUGIN_SOURCES})

# VSTGUI Support
if(SMTG_ENABLE_VSTGUI_SUPPORT)
    target_sources(FlaschenTaschen
        PRIVATE
            resource/myplugineditor.uidesc
    )
    target_link_libraries(FlaschenTaschen
        PRIVATE
            vstgui_support
    )
    smtg_target_add_plugin_resources(FlaschenTaschen
        RESOURCES
            "resource/myplugineditor.uidesc"
    )
endif(SMTG_ENABLE_VSTGUI_SUPPORT)

# Plugin snapshots
smtg_target_add_plugin_snapshots(FlaschenTaschen
    RESOURCES
        resource/4F540AFC9B7A52EC879BF164FBBFFA53_snapshot.png
        resource/4F540AFC9B7A52EC879BF164FBBFFA53_snapshot_2.0x.png
)

# Link against SDK
target_link_libraries(FlaschenTaschen
    PRIVATE
        sdk
)

# Windows-specific: Link Winsock for UDP networking
if(SMTG_WIN)
    target_link_libraries(FlaschenTaschen
        PRIVATE
            ws2_32
    )
endif()

# eSpeak-NG Support (optional)
if(SMTG_ENABLE_ESPEAK_NG)
    # Find eSpeak-NG library
    # You may need to adjust these paths based on your eSpeak-NG installation
    if(WIN32)
        # Default Windows installation paths
        set(ESPEAK_NG_ROOT "C:/Program Files/eSpeak NG" CACHE PATH "Path to eSpeak-NG installation")
        if(EXISTS "${ESPEAK_NG_ROOT}/include")
            target_include_directories(FlaschenTaschen PRIVATE "${ESPEAK_NG_ROOT}/include")
        endif()
        if(EXISTS "${ESPEAK_NG_ROOT}/lib/espeak-ng.lib")
            target_link_libraries(FlaschenTaschen PRIVATE "${ESPEAK_NG_ROOT}/lib/espeak-ng.lib")
        endif()
    else()
        # Unix-like systems (Linux, macOS)
        find_package(PkgConfig)
        if(PkgConfig_FOUND)
            pkg_check_modules(ESPEAK_NG espeak-ng)
            if(ESPEAK_NG_FOUND)
                target_include_directories(FlaschenTaschen PRIVATE ${ESPEAK_NG_INCLUDE_DIRS})
                target_link_libraries(FlaschenTaschen PRIVATE ${ESPEAK_NG_LIBRARIES})
            endif()
        endif()
    endif()

    target_compile_definitions(FlaschenTaschen PRIVATE HAVE_ESPEAK_NG=1)
    message(STATUS "eSpeak-NG support enabled")
else()
    message(STATUS "eSpeak-NG support disabled - TTS will not be available")
endif()

# Version file configuration
smtg_target_configure_version_file(FlaschenTaschen)

# Platform-specific settings
if(SMTG_MAC)
    smtg_target_set_bundle(FlaschenTaschen
        BUNDLE_IDENTIFIER Stratojets.FlaschenTaschen
        COMPANY_NAME "Stratojets"
    )
    smtg_target_set_debug_executable(FlaschenTaschen
        "/Applications/VST3PluginTestHost.app"
        "--pluginfolder;$(BUILT_PRODUCTS_DIR)"
    )
elseif(SMTG_WIN)
    target_sources(FlaschenTaschen PRIVATE
        resource/win32resource.rc
    )
    if(MSVC)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT FlaschenTaschen)

        smtg_target_set_debug_executable(FlaschenTaschen
            "$(ProgramW6432)/Steinberg/VST3PluginTestHost/VST3PluginTestHost.exe"
            "--pluginfolder \"$(OutDir)/\""
        )
    endif()
endif(SMTG_MAC)
