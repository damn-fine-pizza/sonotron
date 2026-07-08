#include "screenshot.hpp"

// stb_image_write.h is vendored with GLFW (third_party/glfw/deps) and is not
// -Werror-clean; this whole translation unit is compiled with warnings off
// (see CMakeLists.txt). It is the only place the implementation is defined.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace sonotron {

bool write_png_rgba_bottom_up(const char* path, int width, int height, const unsigned char* rgba) {
  stbi_flip_vertically_on_write(1);
  const int stride = width * 4;
  return stbi_write_png(path, width, height, 4, rgba, stride) != 0;
}

}  // namespace sonotron
