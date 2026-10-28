
cmake_minimum_required(VERSION 3.22)

project(CHMVoiceMIDI
    VERSION 0.3.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Download JUCE
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.10
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(juce)

# Create Windows application
juce_add_gui_app(CHMVoiceMIDI
    PRODUCT_NAME "CHM Voice MIDI"
    COMPANY_NAME "CHM Studio"
)

# Generate JuceHeader.h
juce_generate_juce_header(CHMVoiceMIDI)

# Source files
target_sources(CHMVoiceMIDI
    PRIVATE
        src/Main.cpp
)

# Compile definitions
target_compile_definitions(CHMVoiceMIDI
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

# Link JUCE modules
target_link_libraries(CHMVoiceMIDI
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
