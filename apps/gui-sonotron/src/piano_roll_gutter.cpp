#include "piano_roll_gutter.hpp"

#include "neon_widgets.hpp"

namespace sonotron {

void draw_piano_key_gutter(ImDrawList* dl, const ImVec2& min, const ImVec2& max, int low_note,
                           int high_note) {
  if (low_note > high_note) {
    return;
  }

  const int rows = high_note - low_note + 1;
  const float row_h = (max.y - min.y) / static_cast<float>(rows);

  for (int n = low_note; n <= high_note; ++n) {
    const float y_top = min.y + static_cast<float>(high_note - n) * row_h;
    const float y_bottom = y_top + row_h;

    // Determine if this is a black key or white key.
    // Black keys: n % 12 in {1, 3, 6, 8, 10} (C#, D#, F#, G#, A#).
    // Safe modulo for potentially negative n: ((n % 12) + 12) % 12.
    const int note_class = ((n % 12) + 12) % 12;
    const bool is_black = (note_class == 1 || note_class == 3 || note_class == 6 ||
                           note_class == 8 || note_class == 10);

    // Choose color: black key (dark) or white key (light).
    const ImVec4 color =
        is_black ? ImVec4(0.06F, 0.07F, 0.09F, 1.0F) : ImVec4(0.72F, 0.75F, 0.78F, 1.0F);

    // Fill the row with the appropriate color (alpha 0.9).
    dl->AddRectFilled(ImVec2(min.x, y_top), ImVec2(max.x, y_bottom), neon::u32(color, 0.9F));

    // Draw a thin divider line after this row, except for the very last row.
    if (n < high_note) {
      dl->AddLine(ImVec2(min.x, y_bottom), ImVec2(max.x, y_bottom),
                  neon::u32(ImVec4(0.15F, 0.16F, 0.18F, 1.0F), 0.6F), 1.0F);
    }
  }
}

}  // namespace sonotron
