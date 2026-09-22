cmake_minimum_required(VERSION 3.22)
project(OMGNEDD VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_subdirectory(JUCE)

juce_add_plugin(OMGNEDD
    COMPANY_NAME            "Yvngboi Nedd"
    BUNDLE_ID               "com.yvngboinedd.omgnedd"
    PLUGIN_MANUFACTURER_CODE Ynde
    PLUGIN_CODE             Omgn
    FORMATS                 VST3
    PRODUCT_NAME            "OMGNEDD"
    VST3_CATEGORIES         Fx Distortion Dynamics EQ
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(OMGNEDD_Resources SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Archivo-SemiBold.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Archivo-ExtraBold.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/IBMPlexMono-Regular.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/IBMPlexMono-SemiBold.ttf")

set_target_properties(OMGNEDD_Resources PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

file(GLOB_RECURSE OMGNEDD_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h")

target_sources(OMGNEDD PRIVATE ${OMGNEDD_SOURCES})
target_include_directories(OMGNEDD PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")

target_compile_definitions(OMGNEDD PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(OMGNEDD
    PRIVATE
        OMGNEDD_Resources
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Build verification: runs the real processor and the real editor.
# ---------------------------------------------------------------------------
juce_add_console_app(OMGNEDD_Tests PRODUCT_NAME "OMGNEDD Tests")

target_sources(OMGNEDD_Tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Tests/PluginTest.cpp"
    ${OMGNEDD_SOURCES})

target_include_directories(OMGNEDD_Tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")

target_compile_definitions(OMGNEDD_Tests PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STANDALONE_APPLICATION=1
    JucePlugin_Name="OMGNEDD"
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0)

target_link_libraries(OMGNEDD_Tests
    PRIVATE
        OMGNEDD_Resources
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags)

# ---------------------------------------------------------------------------
# Optional CPU benchmark, built with -DOMGNEDD_BUILD_BENCH=ON
# ---------------------------------------------------------------------------
if (OMGNEDD_BUILD_BENCH)
    juce_add_console_app(OMGNEDD_Bench PRODUCT_NAME "OMGNEDD Bench")
    target_sources(OMGNEDD_Bench PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tests/Bench.cpp" ${OMGNEDD_SOURCES})
    target_include_directories(OMGNEDD_Bench PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    target_compile_definitions(OMGNEDD_Bench PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0 JUCE_STANDALONE_APPLICATION=1
        JucePlugin_Name="OMGNEDD" JucePlugin_IsSynth=0
        JucePlugin_WantsMidiInput=0 JucePlugin_ProducesMidiOutput=0)
    target_link_libraries(OMGNEDD_Bench PRIVATE OMGNEDD_Resources juce::juce_audio_utils juce::juce_dsp
                          PUBLIC juce::juce_recommended_config_flags)
endif()
