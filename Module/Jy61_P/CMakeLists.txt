cmake_minimum_required(VERSION 3.20)

# 作为独立工程构建时（主机端跑单元测试）才定义 project。
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(jy61 LANGUAGES C)
endif()

if(NOT TARGET jy61)
    add_library(jy61 STATIC
        Src/jy61.c)

    target_include_directories(jy61 PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Inc)

    target_compile_features(jy61 PUBLIC c_std_11)
endif()

# 独立构建或顶层 Module/ 工程开启测试时，才编译主机端测试。
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR OR MODULE_BUILD_TESTS)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(jy61 PRIVATE -Wall -Wextra -Werror)
    endif()
    enable_testing()
    add_subdirectory(Tests)
endif()
