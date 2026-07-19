add_executable(cli_arrangrr main.cpp client_mode.cpp)
set_target_properties(cli_arrangrr PROPERTIES OUTPUT_NAME cli-arrangrr)
target_link_libraries(cli_arrangrr PRIVATE hostrt sonotron_version)
target_compile_options(cli_arrangrr PRIVATE -Wall -Wextra -Werror)
