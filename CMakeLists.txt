cmake_minimum_required(VERSION 3.22)
project(PitchGarden VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build the independently testable pitch estimator without downloading SDKs.
add_library(pitchgarden_core STATIC src/PitchDetector.cpp)
target_include_directories(pitchgarden_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_executable(pitchgarden_pitch_test tests/pitch_test.cpp)
target_link_libraries(pitchgarden_pitch_test PRIVATE pitchgarden_core)
enable_testing()
add_test(NAME pitch_detector COMMAND pitchgarden_pitch_test)

option(PITCHGARDEN_BUILD_PLUGIN "Build ARA VST3 bridge based on the official JUCE ARA demo" OFF)
if(PITCHGARDEN_BUILD_PLUGIN)
  set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE" CACHE PATH "JUCE source checkout")
  set(ARA_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/ARA_SDK" CACHE PATH "ARA SDK checkout")
  if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at ${JUCE_DIR}; run scripts/bootstrap.sh")
  endif()
  if(NOT EXISTS "${ARA_SDK_DIR}/ARA_Library")
    message(FATAL_ERROR "ARA SDK missing at ${ARA_SDK_DIR}; run scripts/bootstrap.sh")
  endif()
  add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/juce")
  juce_set_ara_sdk_path("${ARA_SDK_DIR}")
  juce_add_plugin(PitchGardenARA
    COMPANY_NAME "PitchGarden"
    BUNDLE_ID "org.pitchgarden.pitchgarden"
    PLUGIN_MANUFACTURER_CODE PtGn
    PLUGIN_CODE PgA1
    PRODUCT_NAME "PitchGarden"
    FORMATS VST3
    IS_ARA_EFFECT TRUE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_SYNTH FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)
  juce_generate_juce_header(PitchGardenARA)
  target_sources(PitchGardenARA PRIVATE src/ARAEntry.cpp)
  target_include_directories(PitchGardenARA PRIVATE "${JUCE_DIR}/examples/Plugins")
  target_compile_definitions(PitchGardenARA PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
  target_link_libraries(PitchGardenARA PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
endif()
