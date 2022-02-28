cmake_minimum_required(VERSION 3.22)

project(file)
set(CMAKE_CXX_STANDARD 17)

set(DEBUG_OPTIONS "-Wall" "-Wextra" "-Wpedantic")
set(RELEASE_OPTIONS "-Wall" "-Wextra" "-Wpedantic" "-Werror" "-flto")
add_definitions("-DCATCH_CONFIG_ENABLE_BENCHMARKING=1")

enable_testing()

add_executable(tests
    tests/main.cpp
    tests/file_tests.cpp
)
target_compile_options(tests PUBLIC "$<$<CONFIG:DEBUG>:${DEBUG_OPTIONS}>")
target_compile_options(tests PUBLIC "$<$<CONFIG:RELEASE>:${RELEASE_OPTIONS}>")

file(COPY 
    tests/test.txt
    tests/words
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME tests COMMAND tests)
