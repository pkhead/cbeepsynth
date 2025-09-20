cmake_minimum_required(VERSION 3.22)
project(bpbxsynth VERSION 0.2.0)

set(SOURCES
    src/public.c

    src/synth/synth.c
    src/synth/fm_algo.c
    src/synth/fm.c
    src/synth/wave.c
    src/synth/spectrum.c

    src/fx/volume.c
    src/fx/panning.c
    src/fx/distortion.c
    src/fx/bitcrusher.c
    src/fx/eq.c
    src/fx/chorus.c
    src/fx/echo.c
    src/fx/reverb.c
    src/fx/limiter.c
    
    src/wavetables.c
    src/envelope.c
    src/filtering.c
    src/fft.c
    src/audio.c
    src/context.c
    src/util.c
)

set(TARGET_PROPERTIES
    LANGUAGE C
    C_STANDARD 99
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

set(TARGET_INCLUDES
    PRIVATE src
    PUBLIC include
)

set(TARGET_DEFINITIONS
    BPBXSYN_VERSION_REVISION=${PROJECT_VERSION_PATCH}
)

# static target
if (BEEPBOX_SYNTH_BUILD_STATIC)
    add_library(beepbox_synth_static STATIC ${SOURCES})
    set_target_properties(beepbox_synth_static PROPERTIES ${TARGET_PROPERTIES})
    target_compile_definitions(beepbox_synth_static PUBLIC ${TARGET_DEFINITIONS})
    target_include_directories(beepbox_synth_static PRIVATE ${TARGET_INCLUDES})
    target_include_directories(beepbox_synth_static PUBLIC ${PROJECT_SOURCE_DIR}/include)

    if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(beepbox_synth_static PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

# dynamic target
if (BEEPBOX_SYNTH_BUILD_SHARED)
    add_library(beepbox_synth_shared SHARED ${SOURCES})
    set_target_properties(beepbox_synth_shared PROPERTIES
        ${TARGET_PROPERTIES}
        C_VISIBILITY_PRESET hidden
        OUTPUT_NAME "bpbxsynth"
    )
    target_include_directories(beepbox_synth_shared PRIVATE ${TARGET_INCLUDES})
    target_include_directories(beepbox_synth_shared PUBLIC ${PROJECT_SOURCE_DIR}/include)
    target_compile_definitions(beepbox_synth_shared
        PRIVATE BPBXSYN_SHARED
        PUBLIC ${TARGET_DEFINITIONS}
    )
    
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(beepbox_synth_shared PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()