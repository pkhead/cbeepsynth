cmake_minimum_required(VERSION 3.22)
project(beepbox VERSION 0.1.0)

set(SOURCES
    src/song.c
    src/song_loader.c
)

set(TARGET_PROPERTIES
    LANGUAGE C
    C_STANDARD 11
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

set(TARGET_INCLUDES
    PUBLIC ${CMAKE_SOURCE_DIR}/synth/include
)

set(TARGET_DEFINITIONS
    BPBX_VERSION_REVISION=${PROJECT_VERSION_PATCH}
)

# static target
if (BEEPBOX_BUILD_STATIC)
    add_library(beepbox_static STATIC ${SOURCES})
    set_target_properties(beepbox_static PROPERTIES ${TARGET_PROPERTIES})
    target_compile_definitions(beepbox_static PUBLIC ${TARGET_DEFINITIONS})
    target_include_directories(beepbox_static PRIVATE ${TARGET_INCLUDES})
    target_include_directories(beepbox_static PUBLIC ${PROJECT_SOURCE_DIR}/include)

    if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(beepbox_static PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    if (NOT DEFINED BEEPBOX_SYNTH_LIB)
        message(FATAL_ERROR "BEEPBOX_SYNTH_LIB must be defined to specify how BeepBox should link with it")
    endif()

    target_link_libraries(beepbox_static PUBLIC ${BEEPBOX_SYNTH_LIB})
endif()

# dynamic target
if (BEEPBOX_BUILD_SHARED)
    add_library(beepbox_shared SHARED ${SOURCES})
    set_target_properties(beepbox_shared PROPERTIES
        ${TARGET_PROPERTIES}
        C_VISIBILITY_PRESET hidden
        OUTPUT_NAME "beepbox"
    )
    target_include_directories(beepbox_shared PRIVATE ${TARGET_INCLUDES})
    target_include_directories(beepbox_shared PUBLIC ${PROJECT_SOURCE_DIR}/include)
    target_compile_definitions(beepbox_shared
        PRIVATE BPBX_SHARED
        PUBLIC ${TARGET_DEFINITIONS}
    )
    
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(beepbox_shared PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    
    if (NOT DEFINED BEEPBOX_SYNTH_LIB)
        message(FATAL_ERROR "BEEPBOX_SYNTH_LIB must be defined to specify how BeepBox should link with it")
    endif()

    target_link_libraries(beepbox_shared PUBLIC ${BEEPBOX_SYNTH_LIB})
endif()