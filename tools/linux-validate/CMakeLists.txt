#-----------------------------------------------------------------------------
# tools/linux-validate - build R2Synth on LINUX and run the SDK validator
# and tests/ProcessorTests.cpp against it.
#
# WHY. The session that writes this plug-in reaches the Mac through a
# Linux VM and cannot run Xcode. The VST3 SDK builds on Linux, and so does
# its validator - the same one the Mac build runs as a post-build step -
# so the processor, controller, factory and state can be validated
# without the Mac. The only thing left out is the editor: VSTGUI's Linux
# dependencies (xcb-util, xkbcommon-x11, gtkmm) were not installable in
# the container, so R2SynthEditor.h is replaced by editor-stub.h,
# an EditorView that draws nothing. Every other file is the real source,
# copied unchanged at configure time.
#
#   cmake -S tools/linux-validate -B /tmp/ss-linux -G Ninja \
#         -DVST3_SDK_ROOT=/path/to/vst3sdk -DCMAKE_BUILD_TYPE=Release
#   cmake --build /tmp/ss-linux          # the validator runs post-build
#   /tmp/ss-linux/bin/Release/processortests
#
# NOT the build that ships. The Mac build is ../../CMakeLists.txt.
#-----------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.25)
project(R2SynthLinuxValidate VERSION 1.0.0.1)

set(VST3_SDK_ROOT "" CACHE PATH "Path to a VST3 SDK checkout")
if(NOT EXISTS "${VST3_SDK_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "set -DVST3_SDK_ROOT=/path/to/vst3sdk")
endif()

set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES 0 CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES  0 CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT        0 CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK           0 CACHE BOOL "" FORCE)
set(vst3sdk_SOURCE_DIR "${VST3_SDK_ROOT}")
add_subdirectory(${VST3_SDK_ROOT} ${PROJECT_BINARY_DIR}/vst3sdk)
smtg_enable_vst3_sdk()

# The real sources, less the editor and the files only it uses.
set(_src "${CMAKE_CURRENT_SOURCE_DIR}/../../source")
set(_copy "${PROJECT_BINARY_DIR}/src")
file(REMOVE_RECURSE "${_copy}")
file(GLOB _all "${_src}/*.h" "${_src}/*.cpp")
foreach(_f ${_all})
    get_filename_component(_n "${_f}" NAME)
    if(NOT _n MATCHES "^(R2Synth(Editor|Controls|PanelViews|Layout|Readouts)|LicenceViews)\\.")
        configure_file("${_f}" "${_copy}/${_n}" COPYONLY)
    endif()
endforeach()
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/editor-stub.h" "${_copy}/R2SynthEditor.h" COPYONLY)

file(GLOB plugin_sources "${_copy}/*.cpp" "${_copy}/*.h")
smtg_add_vst3plugin(R2Synth ${plugin_sources})
smtg_target_configure_version_file(R2Synth)
target_compile_features(R2Synth PUBLIC cxx_std_17)
target_include_directories(R2Synth PRIVATE "${_copy}")
target_link_libraries(R2Synth PRIVATE sdk)

# The licence files are copied with the rest; they need Monocypher.
set(_mono "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/monocypher")
target_sources(R2Synth PRIVATE "${_mono}/monocypher.c" "${_mono}/monocypher-ed25519.c")
target_include_directories(R2Synth PRIVATE "${_mono}")

# The processor tests: the processor's own sources, no controller, no
# module - the processor is driven directly.
add_executable(processortests
    "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/ProcessorTests.cpp"
    "${_copy}/R2SynthProcessor.cpp"
    "${_copy}/R2SynthParams.cpp"
    "${_copy}/R2SynthDsp.cpp"
    # MemoryStream is in the SDK's source tree but not in its libraries.
    "${VST3_SDK_ROOT}/public.sdk/source/common/memorystream.cpp")
target_include_directories(processortests PRIVATE "${_copy}")
target_compile_features(processortests PUBLIC cxx_std_17)
target_link_libraries(processortests PRIVATE sdk sdk_hosting)
