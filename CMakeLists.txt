cmake_minimum_required(VERSION 3.22)
project(beepbox_project)

if (BEEPBOX_BUILD_TESTS)
    set(BEEPBOX_SYNTH_BUILD_STATIC TRUE)
    set(BEEPBOX_BUILD_STATIC TRUE)
    set(BEEPBOX_SYNTH_LIB beepbox_synth_static)
endif()

add_subdirectory(synth)
add_subdirectory(song)

if (BEEPBOX_BUILD_TESTS)
    add_subdirectory(tests)
endif()