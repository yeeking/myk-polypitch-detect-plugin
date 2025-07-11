## MYK claiming minimal credit for the hard stuff in this project: see https://github.com/DamRsn/NeuralNote 
## for the clever bits I re-purposed

cmake_minimum_required(VERSION 3.16)
project(MykPolyPitch VERSION 1.1.0)

# C++ settings
set(CMAKE_CXX_STANDARD 17)
enable_language(CXX)

#Compile commands, useful for some IDEs like VS-Code
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

#Minimum MacOS target, set globally
# set(CMAKE_OSX_DEPLOYMENT_TARGET "10.11" CACHE STRING "Minimum OS X deployment version" FORCE)
# 10.15 for my filesystem.exists function
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version" FORCE)

option(UniversalBinary "Build universal binary for mac" OFF)
option(RTNeural_Release "When CMAKE_BUILD_TYPE=Debug, overwrite it to Release for RTNeural only" OFF)
option(LTO "Enable Link Time Optimization" ON)

if (UniversalBinary)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE INTERNAL "")
    message(STATUS "Building Universal Binary")
endif ()

#static linking in Windows
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_subdirectory(ThirdParty/JUCE)
add_subdirectory(ThirdParty/RTNeural)

set(BaseTargetName MykPolyPitch)

# Set COPY_PLUGIN_AFTER_BUILD to TRUE on Mac and to FALSE on Windows due to permission issues
set(COPY_PLUGIN_AFTER_BUILD TRUE)

if (WIN32)
    set(COPY_PLUGIN_AFTER_BUILD FALSE)
endif ()

juce_add_plugin("${BaseTargetName}"
        # VERSION ...                               # Set this if the plugin version is different to the project version
#        ICON_BIG "${CMAKE_CURRENT_LIST_DIR}/src/assets/logo.png"
#        ICON_SMALL "${CMAKE_CURRENT_LIST_DIR}/src/assets/logo.png"
        COMPANY_NAME "Yee-King"
        BUNDLE_ID "net.yeeking.polypitch"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT TRUE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD ${COPY_PLUGIN_AFTER_BUILD}
        MICROPHONE_PERMISSION_ENABLED TRUE
        MICROPHONE_PERMISSION_TEXT "Need access to Microphone"
        HARDENED_RUNTIME_ENABLED TRUE
        PLUGIN_MANUFACTURER_CODE YeeK
        PLUGIN_CODE PPVO
        FORMATS AU VST3 Standalone
        PRODUCT_NAME "MykPolyPitch")

juce_generate_juce_header(${BaseTargetName})

# Source files

file(GLOB_RECURSE SOURCES_PLUGIN ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp) # not lib as now its a sub dir

list(REMOVE_ITEM SOURCES_PLUGIN ${CMAKE_CURRENT_LIST_DIR}/src/lib/Model/BasicPitchCNN.cpp)
#file(GLOB_RECURSE HEADERS_PLUGIN ${CMAKE_CURRENT_LIST_DIR}/src/*.h ${CMAKE_CURRENT_LIST_DIR}/Lib/*.h)
file(GLOB_RECURSE HEADERS_PLUGIN ${CMAKE_CURRENT_LIST_DIR}/src/*.h)

list(REMOVE_ITEM HEADERS_PLUGIN ${CMAKE_CURRENT_LIST_DIR}/src/lib/Model/BasicPitchCNN.h)

target_sources(${BaseTargetName} PRIVATE ${SOURCES_PLUGIN} ${HEADERS_PLUGIN})

file(GLOB_RECURSE lib_dirs LIST_DIRECTORIES true ${CMAKE_CURRENT_LIST_DIR}/src/lib/*)

foreach (dir ${lib_dirs})
    IF (IS_DIRECTORY ${dir})
        target_include_directories(${BaseTargetName} PRIVATE ${dir})
    ELSE ()
        CONTINUE()
    ENDIF ()
endforeach ()

file(GLOB_RECURSE source_dirs LIST_DIRECTORIES true ${CMAKE_CURRENT_LIST_DIR}/src/*)

foreach (dir ${source_dirs})
    IF (IS_DIRECTORY ${dir})
        target_include_directories(${BaseTargetName} PRIVATE ${dir})
    ELSE ()
        CONTINUE()
    ENDIF ()
endforeach ()

#Binary data
file(GLOB RESOURCES_FILES ${CMAKE_CURRENT_LIST_DIR}/src/assets/ModelData/*.json
        ${CMAKE_CURRENT_LIST_DIR}/src/assets/ModelData/*.ort
        ${CMAKE_CURRENT_LIST_DIR}/src/assets/*.ttf
        ${CMAKE_CURRENT_LIST_DIR}/src/assets/*.png
        ${CMAKE_CURRENT_LIST_DIR}/src/assets/*.svg)

juce_add_binary_data(bin_data SOURCES ${RESOURCES_FILES})

add_library(BasicPitchCNN STATIC ${CMAKE_CURRENT_LIST_DIR}/src/lib/Model/BasicPitchCNN.cpp)
target_include_directories(BasicPitchCNN PRIVATE ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/RTNeural)
if ((CMAKE_BUILD_TYPE STREQUAL "Debug") AND RTNeural_Release)
    if (MSVC)
        target_compile_options(BasicPitchCNN PUBLIC /O2) # or maybe /Ox
    else () # clang or gcc
        target_compile_options(BasicPitchCNN PUBLIC -O3) # or maybe -Ofast
    endif ()
endif ()
target_link_libraries(BasicPitchCNN PUBLIC RTNeural bin_data)

target_include_directories(${BaseTargetName} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/ONNXRuntime/${ONNXRUNTIME_DIRNAME}/include)
target_include_directories(${BaseTargetName} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/minimp3)

target_compile_definitions(${BaseTargetName}
        PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

add_library(onnxruntime STATIC IMPORTED)

if (APPLE)
    set_property(TARGET onnxruntime PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/onnxruntime/lib/libonnxruntime.a)

elseif (WIN32)
    set_property(TARGET onnxruntime APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)

    set_target_properties(onnxruntime PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
            IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/onnxruntime/lib/onnxruntime.lib"
    )
    set_target_properties(onnxruntime PROPERTIES
            MAP_IMPORTED_CONFIG_DEBUG Release
            MAP_IMPORTED_CONFIG_MINSIZEREL Release
            MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
    )
elseif (UNIX)
    set_property(TARGET onnxruntime PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/onnxruntime/lib/libonnxruntime.a)
endif ()


target_include_directories("${BaseTargetName}" PRIVATE ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/onnxruntime/include)


target_link_libraries(${BaseTargetName}
        PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        BasicPitchCNN
        onnxruntime
        bin_data
        PUBLIC
        juce_recommended_config_flags)

if (${LTO})
    target_link_libraries(${BaseTargetName} PUBLIC juce_recommended_lto_flags)
    message(STATUS "Link Time Optimization (LTO) enabled")
endif ()

if (BUILD_UNIT_TESTS)
    add_subdirectory(Tests)
endif ()

target_compile_definitions(${BaseTargetName}
        PRIVATE
        SAVE_DOWNSAMPLED_AUDIO=0
        USE_TEST_NOTE_FRAME_TO_TIME=0
)

if (WIN32)
    target_compile_definitions(${BaseTargetName} PUBLIC JUCE_ASIO=1)
    target_include_directories(${BaseTargetName} PRIVATE ThirdParty/ASIO/common)
endif ()



######## Test app

juce_add_console_app(polypitch-tests)

juce_generate_juce_header(polypitch-tests)

file(GLOB_RECURSE SOURCES_TEST ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp) 
# file(GLOB_RECURSE SOURCES_TEST ${CMAKE_CURRENT_LIST_DIR}/src/*.c) 

list(REMOVE_ITEM SOURCES_TEST ${CMAKE_CURRENT_LIST_DIR}/src/lib/Model/BasicPitchCNN.cpp)
list(REMOVE_ITEM SOURCES_TEST ${CMAKE_CURRENT_LIST_DIR}/src/plugin/PluginProcessor.cpp)
list(REMOVE_ITEM SOURCES_TEST ${CMAKE_CURRENT_LIST_DIR}/src/plugin/PluginEditor.cpp)

message(STATUS "SOURCES_TEST contains: ${SOURCES_TEST}")

target_sources(polypitch-tests PRIVATE ${SOURCES_TEST} ${HEADERS_PLUGIN})


target_compile_definitions(polypitch-tests
    PRIVATE
        JUCE_ALSA=1
        JUCE_DIRECTSOUND=1
        JUCE_USE_OGGVORBIS=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_UNIT_TESTS=1  # Good to enable JUCE's built-in unit-testing support
)


target_include_directories(polypitch-tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/onnxruntime/include)
target_include_directories(polypitch-tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/ONNXRuntime/${ONNXRUNTIME_DIRNAME}/include)
target_include_directories(polypitch-tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/minimp3)
target_include_directories(polypitch-tests
    PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}  # This is where JuceHeader.h is generated
)

foreach (dir ${lib_dirs})
    IF (IS_DIRECTORY ${dir})
        target_include_directories(polypitch-tests PRIVATE ${dir})
    ELSE ()
        CONTINUE()
    ENDIF ()
endforeach ()

target_link_libraries(polypitch-tests
    PRIVATE
        juce::juce_audio_utils
        # juce::juce_core
        # juce::juce_events
        # juce::juce_audio_basics
       juce::juce_dsp
        # llama  # if needed
        BasicPitchCNN
        onnxruntime
        bin_data
        juce::juce_recommended_config_flags
        # juce::juce_recommended_lto_flags
        # juce::juce_recommended_warning_flags
)
