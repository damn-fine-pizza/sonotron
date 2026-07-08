#pragma once

#include <array>
#include <cstddef>

// Pure-data view model for the Intention zone — the "podium" of expressive
// axes the Director is steering. Deliberately ImGui-free and core-free (like
// layout_model.hpp): this is exactly what the panel draws and, once the
// brain is wired (D38 needs a client channel that does not exist yet), what
// the core will populate. Unit-testable on its own; the ImGui drawing lives
// in intention_panel.hpp/.cpp.

namespace sonotron {

// One expressive axis. `current` is where the music IS right now; `target`
// is where the Director wants it to GO. Both normalized to [0, 1]. `label`
// is a static string literal (uppercase, for the panel's fixed-width column).
struct IntentionAxis {
  const char* label = "";
  float current = 0.0F;  // [0, 1]
  float target = 0.0F;   // [0, 1]
};

// The three axes of the podium, in fixed display order: energy, tension,
// valence. A plain array so the renderer can iterate uniformly; the index
// meaning is fixed by kEnergy/kTension/kValence below.
struct IntentionState {
  std::array<IntentionAxis, 3> axes{};
};

inline constexpr std::size_t kEnergy = 0;
inline constexpr std::size_t kTension = 1;
inline constexpr std::size_t kValence = 2;

// Fixed placeholder values used to nail the panel's look before the brain is
// connected. Deliberately asymmetric per axis (each with a visible
// current->target gap, valence even moving downward) so every row exercises
// the drawing.
IntentionState mock_intention_state();

// Clamps a raw axis value into the drawable [0, 1] range. Lives here (not in
// the panel) so it is testable without ImGui and shared by current + target.
float clamp_unit(float value);

}  // namespace sonotron
