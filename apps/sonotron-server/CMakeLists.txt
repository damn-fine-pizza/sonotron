add_executable(sonotron_server main.cpp)
set_target_properties(sonotron_server PROPERTIES OUTPUT_NAME sonotron-server)
target_link_libraries(sonotron_server PRIVATE hostrt)
target_compile_options(sonotron_server PRIVATE -Wall -Wextra -Werror)
