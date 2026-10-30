cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(zigbee LANGUAGES C)
endif()

if(NOT TARGET zigbee)
    add_library(zigbee STATIC
        Src/zigbee.c
    )
    target_include_directories(zigbee PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Inc)
    target_compile_features(zigbee PUBLIC c_std_11)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR OR MODULE_BUILD_TESTS)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(zigbee PRIVATE -Wall -Wextra -Werror)
    endif()
    enable_testing()
    add_subdirectory(Tests)
endif()
