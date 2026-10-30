add_executable(test_jy61 test_jy61.c)

target_link_libraries(test_jy61 PRIVATE jy61)
target_compile_features(test_jy61 PRIVATE c_std_11)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jy61 PRIVATE -Wall -Wextra -Werror)
endif()

add_test(NAME jy61 COMMAND test_jy61)
