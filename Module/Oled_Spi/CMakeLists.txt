cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(oled LANGUAGES C)
endif()

if(NOT TARGET oled)
    add_library(oled STATIC
        Src/oled.c
        Src/oled_font.c)

    target_include_directories(oled PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Inc)

    target_compile_features(oled PUBLIC c_std_11)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR OR MODULE_BUILD_TESTS)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(oled PRIVATE -Wall -Wextra -Werror)
    endif()
    enable_testing()
    add_subdirectory(Tests)
endif()
