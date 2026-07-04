#include <iostream>
#include <string>
#include <vector>

#include "cli.hpp"

// Thin entry point: all logic lives in arrstyle::run so tests can drive whole
// subcommands and assert exit codes without spawning a process.
int main(int argc, char** argv) {
  std::vector<std::string> args;
  args.reserve(static_cast<std::size_t>(argc > 1 ? argc - 1 : 0));
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }
  return arrstyle::run(args, std::cout, std::cerr);
}
