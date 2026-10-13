cmake_minimum_required(VERSION 3.22)
project(PlaitsExtendedPlugin VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

option(PLAITS_TESTS_ONLY "Build only the headless DSP tests (no JUCE, no plug-in)" OFF)

if(NOT PLAITS_TESTS_ONLY)
include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.6
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# Plugin formats: AU + VST3 (+ Standalone for quick testing).
set(FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND FORMATS AU)
endif()

# Like Plaits, this has no audio input: it is a plain instrument, so it shows up in Logic's normal Instrument slot.
juce_add_plugin(PlaitsExtended
  COMPANY_NAME "particledots"
  PLUGIN_MANUFACTURER_CODE Ptdt
  PLUGIN_CODE PlEx
  FORMATS ${FORMATS}
  PRODUCT_NAME "Unofficial Plaits Extended"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE)
endif()

# --- DSP core: vendored Plaits Palette/stmlib code + our JUCE-free wrapper, built with plain flags ---
# The source list (118 files for the 95 models) is generated from the Palette catalog by tools/gen_models.py.
include(cmake/PlaitsSources.cmake)
add_library(plaits_core STATIC src/PlaitsEngine.cpp ${PLAITS_DSP_SOURCES})
target_include_directories(plaits_core PUBLIC src third_party)
# TEST=1 is the upstream desktop-build switch: it turns the STM32 flash / assembly code into portable stand-ins.
target_compile_definitions(plaits_core PUBLIC TEST=1 PLAITS_ENGINE_COUNT=${PLAITS_MODEL_COUNT})
set_target_properties(plaits_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
# Some upstream headers rely on <cstdio>/<cstdlib>/<cmath>/<cstdint> being pulled in transitively.
if(MSVC)
  target_compile_options(plaits_core PRIVATE /FIcstdio /FIcstdlib /FIcmath /FIcstdint)
else()
  target_compile_options(plaits_core PRIVATE "SHELL:-include cstdio" "SHELL:-include cstdlib" "SHELL:-include cmath" "SHELL:-include cstdint" -Wno-unused-local-typedef -Wno-unused-variable)
endif()

if(NOT PLAITS_TESTS_ONLY)
# --- The plugin ---
target_sources(PlaitsExtended PRIVATE src/PluginProcessor.cpp src/PluginEditor.cpp)
target_compile_definitions(PlaitsExtended PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(PlaitsExtended PRIVATE
  plaits_core
  juce::juce_audio_utils
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)

# --- Processor-level test (parameters, MIDI -> voice, state), no host or window needed:  build/plugin_test ---
add_executable(plugin_test EXCLUDE_FROM_ALL src/plugin_test.cpp)
target_compile_definitions(plugin_test PRIVATE JucePlugin_Name="Unofficial Plaits Extended" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(plugin_test PRIVATE PlaitsExtended juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()

# --- Headless test of the DSP wrapper:  cmake --build build --target engine_test && build/engine_test ---
add_executable(engine_test EXCLUDE_FROM_ALL src/engine_test.cpp)
target_link_libraries(engine_test PRIVATE plaits_core)

# --- Engine-switching regression test:  JUNK=0xFF build/sweep_test ---
add_executable(sweep_test EXCLUDE_FROM_ALL src/sweep_test.cpp)
target_link_libraries(sweep_test PRIVATE plaits_core)

# --- Shared-RAM check: every model's engine must fit in the block the plug-in provides:  build/ram_probe ---
add_executable(ram_probe EXCLUDE_FROM_ALL src/ram_probe.cpp)
target_link_libraries(ram_probe PRIVATE plaits_core)
