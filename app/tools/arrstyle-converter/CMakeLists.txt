# arrstyle-converter: HOST-ONLY importer/compiler tooling. It is wired into the
# top-level build only inside the non-firmware branch, so it never enters the
# ARM preset and never touches the core/firmware builds. Dependency-free: it
# links no third-party library and does not link arrangrr_core.

add_library(arrstyle_lib STATIC
  src/model.cpp
  src/diagnostics.cpp
  src/json.cpp
  src/serialize.cpp
  src/smf.cpp
  src/genre.cpp
  src/canon.cpp
  src/midi_import.cpp
  src/chordpro_import.cpp
  src/sff_import.cpp
  src/validate.cpp
  src/cli.cpp
)
target_include_directories(arrstyle_lib PUBLIC src)
target_compile_options(arrstyle_lib PRIVATE -Wall -Wextra -Werror)

add_executable(arrstyle_converter src/main.cpp)
set_target_properties(arrstyle_converter PROPERTIES OUTPUT_NAME arrstyle-converter)
target_link_libraries(arrstyle_converter PRIVATE arrstyle_lib)
target_compile_options(arrstyle_converter PRIVATE -Wall -Wextra -Werror)

# Tests are host-only (this whole subtree is added only in the non-firmware
# branch of the top-level CMakeLists, where enable_testing() is in effect).
add_subdirectory(tests)
