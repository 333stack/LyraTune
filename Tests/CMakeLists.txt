# Native (Linux) unit tests for the framework-independent DSP in Source/DSP.
# Usage: cmake -S Tests -B build/tests -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C build/tests && build/tests/dsp_tests
cmake_minimum_required(VERSION 3.22)
project(LyraDspTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DSP ${CMAKE_CURRENT_SOURCE_DIR}/../Source/DSP)
add_executable(dsp_tests
    dsp_tests.cpp
    ${DSP}/Pitch/TargetSelector.cpp
    ${DSP}/Pitch/YinDetector.cpp
    ${DSP}/Pitch/PitchShifter.cpp
    ${DSP}/Pitch/CorrectionEngine.cpp
    ${DSP}/TunerChain.cpp)
target_include_directories(dsp_tests PRIVATE ${DSP})
target_compile_options(dsp_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wno-sign-conversion)

option(LYRA_SANITIZE "Build tests with ASan/UBSan" OFF)
if(LYRA_SANITIZE)
    target_compile_options(dsp_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(dsp_tests PRIVATE -fsanitize=address,undefined)
endif()

add_executable(analyze_vocal ${CMAKE_CURRENT_SOURCE_DIR}/../Tools/analyze_vocal.cpp ${DSP}/Pitch/YinDetector.cpp)
target_include_directories(analyze_vocal PRIVATE ${DSP})

add_executable(render_tuner ${CMAKE_CURRENT_SOURCE_DIR}/../Tools/render_tuner.cpp
    ${DSP}/Pitch/TargetSelector.cpp ${DSP}/Pitch/YinDetector.cpp ${DSP}/Pitch/PitchShifter.cpp
    ${DSP}/Pitch/CorrectionEngine.cpp ${DSP}/TunerChain.cpp)
target_include_directories(render_tuner PRIVATE ${DSP})
