add_executable(test_oled test_oled.c)

target_link_libraries(test_oled PRIVATE oled)
target_compile_features(test_oled PRIVATE c_std_11)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_oled PRIVATE -Wall -Wextra -Werror)
endif()

add_test(NAME oled COMMAND test_oled)
