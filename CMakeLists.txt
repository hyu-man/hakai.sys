cmake_minimum_required(VERSION 3.22)

project(HAKAI_SYS VERSION 0.1.0)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE JUCE)

juce_add_plugin(HakaiSys
    COMPANY_NAME "vicugna"
    PLUGIN_MANUFACTURER_CODE Vcgn
    PLUGIN_CODE Hks1
    FORMATS VST3 Standalone
    PRODUCT_NAME "hakai.sys"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(HakaiSysData SOURCES
    Assets/DotGothic16-Regular.ttf)

target_sources(HakaiSys PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_options(HakaiSys PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_compile_definitions(HakaiSys PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(HakaiSys
    PRIVATE
        HakaiSysData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
