cmake_minimum_required(VERSION 3.22)
project(PitchGarden VERSION 0.2.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build the independently testable pitch estimator without downloading SDKs.
add_library(pitchgarden_core STATIC src/PitchDetector.cpp src/PitchAnalysis.cpp src/AnalysisModel.cpp src/NoteSegmenter.cpp)
target_include_directories(pitchgarden_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_executable(pitchgarden_pitch_test tests/pitch_test.cpp)
target_link_libraries(pitchgarden_pitch_test PRIVATE pitchgarden_core)
find_package(Threads REQUIRED)
target_link_libraries(pitchgarden_core PUBLIC Threads::Threads)
add_executable(pitchgarden_model_test tests/model_test.cpp)
target_link_libraries(pitchgarden_model_test PRIVATE pitchgarden_core)
add_test(NAME analysis_lifecycle COMMAND pitchgarden_model_test)
set_target_properties(pitchgarden_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_executable(pitchgarden_analysis_test tests/analysis_test.cpp)
target_link_libraries(pitchgarden_analysis_test PRIVATE pitchgarden_core)
add_executable(pitchgarden_segment_test tests/segment_test.cpp)
target_link_libraries(pitchgarden_segment_test PRIVATE pitchgarden_core)
add_executable(pitchgarden_playback_test tests/playback_test.cpp)
target_link_libraries(pitchgarden_playback_test PRIVATE pitchgarden_core)
add_executable(pitchgarden_render_gate_test tests/render_gate_test.cpp)
target_link_libraries(pitchgarden_render_gate_test PRIVATE pitchgarden_core)
enable_testing()
add_test(NAME render_gate COMMAND pitchgarden_render_gate_test)
add_test(NAME dry_playback COMMAND pitchgarden_playback_test)
add_test(NAME note_segmentation COMMAND pitchgarden_segment_test)
add_test(NAME pitch_analysis COMMAND pitchgarden_analysis_test)
add_test(NAME pitch_detector COMMAND pitchgarden_pitch_test)

option(PITCHGARDEN_BUILD_PLUGIN "Build the PitchGarden analysis ARA VST3 plugin" OFF)
if(PITCHGARDEN_BUILD_PLUGIN)
  set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE" CACHE PATH "JUCE source checkout")
  set(ARA_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/ARA_SDK" CACHE PATH "ARA SDK checkout")
  if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at ${JUCE_DIR}; run scripts/bootstrap.sh")
  endif()
  if(NOT EXISTS "${ARA_SDK_DIR}/ARA_Library/PlugIn/ARAPlug.h"
     OR NOT EXISTS "${ARA_SDK_DIR}/ARA_API/ARAInterface.h")
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
  target_sources(PitchGardenARA PRIVATE src/ARAEntry.cpp src/ARAAnalysisBridge.cpp src/PitchEditor.cpp)
  target_compile_definitions(PitchGardenARA PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
  target_link_libraries(PitchGardenARA PRIVATE
    pitchgarden_core
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
  option(PITCHGARDEN_BUILD_PLUGIN_TESTS "Build in-process ARA integration regression test" ON)
  if(PITCHGARDEN_BUILD_PLUGIN_TESTS)
    add_executable(pitchgarden_ara_test tests/ara_integration_test.cpp
      "${JUCE_DIR}/modules/juce_audio_plugin_client/juce_audio_plugin_client_ARA.cpp")
    target_include_directories(pitchgarden_ara_test PRIVATE
      $<TARGET_PROPERTY:PitchGardenARA,INCLUDE_DIRECTORIES>)
    target_compile_definitions(pitchgarden_ara_test PRIVATE
      $<TARGET_PROPERTY:PitchGardenARA,COMPILE_DEFINITIONS>)
    target_link_libraries(pitchgarden_ara_test PRIVATE PitchGardenARA pitchgarden_core)
    add_test(NAME ara_integration COMMAND pitchgarden_ara_test)
    set_tests_properties(ara_integration PROPERTIES TIMEOUT 30)
  endif()
endif()
