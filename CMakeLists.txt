cmake_minimum_required(VERSION 3.22)
project(beepbox_synth VERSION 0.1.0)

set(SOURCES
    src/public.c

    src/instrument.c
    src/fm_algo.c
    src/fm.c
    src/chip.c
    src/harmonics.c
    
    src/wavetables.c
    src/envelope.c
    src/filtering.c
    src/fft.c
)

set(TARGET_PROPERTIES
    LANGUAGE C
    C_STANDARD 11
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

set(TARGET_INCLUDES
    PRIVATE src
    PUBLIC include
)

# static target
if (BEEPBOX_SYNTH_BUILD_STATIC)
    add_library(beepbox_synth_static STATIC ${SOURCES})
    set_target_properties(beepbox_synth_static PROPERTIES ${TARGET_PROPERTIES})
    target_include_directories(beepbox_synth_static ${TARGET_INCLUDES})

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
        OUTPUT_NAME "beepboxsynth"
    )
    target_include_directories(beepbox_synth_shared ${TARGET_INCLUDES})
    target_compile_definitions(beepbox_synth_shared PRIVATE BPBX_SHARED)
    
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(beepbox_synth_shared PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()