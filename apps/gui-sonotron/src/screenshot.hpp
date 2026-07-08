#pragma once

// Host-only verification helper: write a captured framebuffer to a PNG so a
// GUI frame can be inspected without a running display server (see the
// SONOTRON_GUI_SCREENSHOT escape hatch in main.cpp, sibling to
// SONOTRON_GUI_MAX_FRAMES). Thin wrapper over the stb_image_write.h already
// vendored under third_party/glfw/deps — no new dependency.

namespace sonotron {

// Writes a tightly-packed RGBA8 buffer to `path` as a PNG. The buffer is
// treated as BOTTOM-UP (OpenGL glReadPixels origin) and flipped on write, so
// the resulting image is right-side-up. Returns false on write failure.
bool write_png_rgba_bottom_up(const char* path, int width, int height, const unsigned char* rgba);

}  // namespace sonotron
