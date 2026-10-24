cmake_minimum_required(VERSION 3.22)
project(LyraTune VERSION 0.2.0 LANGUAGES C CXX)

# Product naming lives here only; DSP code never refers to it.
set(PRODUCT_NAME "Lyra Tune" CACHE STRING "User-visible plugin name")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static release C runtime (/MT) in all configs: no runtime DLLs needed on the target, and xwin ships no debug runtime.
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)

add_subdirectory(third_party/JUCE)

juce_add_plugin(LyraTune
    COMPANY_NAME "stack"
    BUNDLE_ID com.stack.lyratune
    PLUGIN_MANUFACTURER_CODE Lyra
    PLUGIN_CODE LyTn
    PRODUCT_NAME "${PRODUCT_NAME}"
    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    VST3_CATEGORIES Fx Pitch Shift
    VST3_AUTO_MANIFEST FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(LyraTuneAssets SOURCES
    Resources/fonts/Inter-Regular.ttf
    Resources/fonts/Inter-SemiBold.ttf)

target_sources(LyraTune PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/Theme.cpp
    Source/UI/LyraLookAndFeel.cpp
    Source/UI/Knob.cpp
    Source/UI/Orb.cpp
    Source/UI/KeyboardPanel.cpp
    Source/UI/Widgets.cpp
    Source/DSP/Pitch/TargetSelector.cpp
    Source/DSP/Pitch/YinDetector.cpp
    Source/DSP/Pitch/PitchShifter.cpp
    Source/DSP/Pitch/CorrectionEngine.cpp
    Source/DSP/TunerChain.cpp)

target_compile_definitions(LyraTune PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(LyraTune
    PRIVATE LyraTuneAssets juce::juce_audio_processors
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Dev tool: render the editor to PNG under Wine (not installed, not part of the plugin).
add_executable(ui_snapshot Tools/ui_snapshot.cpp)
target_include_directories(ui_snapshot PRIVATE $<TARGET_PROPERTY:LyraTune,INCLUDE_DIRECTORIES>)
target_compile_definitions(ui_snapshot PRIVATE $<TARGET_PROPERTY:LyraTune,COMPILE_DEFINITIONS>)
target_link_libraries(ui_snapshot PRIVATE LyraTune juce::juce_recommended_config_flags)
set_target_properties(ui_snapshot PROPERTIES WIN32_EXECUTABLE FALSE)
