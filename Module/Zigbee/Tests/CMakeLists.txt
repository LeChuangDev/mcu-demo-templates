add_executable(test_zigbee test_zigbee.c)
target_link_libraries(test_zigbee PRIVATE zigbee)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_zigbee PRIVATE -Wall -Wextra -Werror)
endif()

add_test(NAME zigbee COMMAND test_zigbee)
