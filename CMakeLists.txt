cmake_minimum_required(VERSION 3.22)

project(file)
set(CMAKE_CXX_STANDARD 17)

if(MSVC)
    set(DEBUG_OPTIONS /W4 /external:I${CMAKE_CURRENT_SOURCE_DIR}/tests/catch /external:W0)
    set(RELEASE_OPTIONS ${DEBUG_OPTIONS} /WX)
    add_definitions("-DWIN32_LEAN_AND_MEAN=1")
else()
    set(DEBUG_OPTIONS -Wall -Wextra -Wpedantic -isystem ${CMAKE_CURRENT_SOURCE_DIR}/tests/catch)
    set(RELEASE_OPTIONS ${DEBUG_OPTIONS} -Werror)
endif()
add_definitions("-DCATCH_CONFIG_ENABLE_BENCHMARKING=1")

enable_testing()

add_executable(tests
    tests/main.cpp
    tests/file_tests.cpp
)
target_compile_options(tests PUBLIC "$<$<CONFIG:DEBUG>:${DEBUG_OPTIONS}>")
target_compile_options(tests PUBLIC "$<$<CONFIG:RELEASE>:${RELEASE_OPTIONS}>")

add_custom_command(TARGET tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_SOURCE_DIR}/tests/test_data/test.txt
    ${CMAKE_SOURCE_DIR}/tests/test_data/words
    $<TARGET_FILE_DIR:tests>)
add_test(NAME tests COMMAND tests WORKING_DIRECTORY $<TARGET_FILE_DIR:tests>)
