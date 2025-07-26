project(beepbox_tests)

add_executable(beepbox_tests
    tests.c
    munit/munit.c
)

target_link_libraries(beepbox_tests PUBLIC beepbox_static)