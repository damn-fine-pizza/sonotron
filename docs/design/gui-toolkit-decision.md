# GUI Toolkit & Dependency Decision (node 11600, Phase 2)

Status: **AWAITING OWNER APPROVAL of the concrete dependency set (policy 0800).** No toolkit or
backend is installed, vendored, or scaffolded until this is approved.

The toolkit itself is **not** re-opened here: **D38 (commit `a092e21`) already locked Dear ImGui**
as the desktop GUI client — *"Rejected JUCE/web/Tauri/Qt with rationale."* Phase 2 therefore is a
**reconciliation + concrete-backend naming**, not a fresh bake-off. Inputs: `corelli-architecture-
critic` (pure-client fit + integration shape) and a 2026 web survey of ImGui backends.

---

## 1. The decision to approve

**Adopt this dependency set for a NEW host-only GUI target (`app/gui/`, provisional):**

| Component | What | Source / weight on Fedora | License |
|---|---|---|---|
| **Dear ImGui** | the UI toolkit (D38) | vendored — git submodule of `ocornut/imgui` (core `.cpp/.h` + the two chosen backend files). *Never a distro package anywhere; vendoring is unavoidable regardless of backend.* | MIT |
| **GLFW3** | window + GL context + input | stock Fedora `glfw-devel` (`find_package(glfw3)`); CMake `FetchContent` fallback for macOS/Windows later | zlib/libpng |
| **imgui_impl_glfw + imgui_impl_opengl3** | the ImGui backend pair | from the ImGui submodule (upstream-maintained, first-class) | MIT |
| **system OpenGL** | renderer | already present | — |

**Recommendation: GLFW + OpenGL3**, not SDL2/SDL3 and not hello_imgui. Reasons (both agents agree):
- **Narrowest footprint.** GLFW does one job — window/context/input. SDL2/3 are full multimedia
  layers (audio, gamepad, haptics) this MIDI-only, socket-driven app will never touch — exactly the
  "dependency wider than needed" that policy 0800 exists to avoid.
- **Wayland-native.** Fedora Workstation (the dev's platform) defaults to Wayland; GLFW supports it
  natively. (This also disqualifies the lighter `sokol_app`, which is X11-only by design.)
- **First-class & maintained.** `imgui_impl_glfw` is used in most of upstream ImGui's own examples;
  actively patched through 2026.
- **Cross-platform later is not sacrificed.** GLFW covers Linux/macOS/Windows with one API, so the
  "macOS/Windows later" goal is equally reachable as with SDL.
- **hello_imgui rejected:** its value (one-line mobile/web/store packaging, asset embedding) targets
  platforms this project deprioritized; it adds a framework layer with its own pinned ImGui and idle
  model to work around — surface for capabilities we don't need.
- **SDL3 > SDL2** *if* the SDL family were chosen (Fedora is migrating under SDL2; upstream labels
  SDL3 "Recommended") — but neither beats GLFW's narrower footprint here.

None of the three candidates risks the GUI linking the core — they are pure window/render libs,
isolated from the boundary concern in §3.

---

## 2. Integration shape (Corelli — architecture, decided seam)

- **Single binary, single thread, poll-in-frame.** The ImGui render loop (~60 fps, ~16 ms budget)
  drains the non-blocking UDS socket fd (`recv`/`poll`, `MSG_DONTBLOCK`, to `EAGAIN`) once per
  frame. **No background reader thread** — it would add synchronization, a second failure point, and
  a drop-policy to reinvent, to protect a guarantee the server does not offer anyway (its broadcast
  is already best-effort/lossy). Max added latency = one frame; zero risk of the GUI entering the
  timing path. This mirrors the server's own poll pattern in `main.cpp`.

---

## 3. Pure-client boundary — rules the new target MUST obey

Confirmed sound **on the wire** (only text L1 out / JSONL in cross the socket). The real trap is
**host-side code reuse**, and the graph confirms it:
- `jsonl.cpp` (which renders the JSONL) **is coupled to the core** — it includes `chord_engine.hpp`
  / `theory.hpp` / `transport.hpp` and lives in `arrangrr_host`, which links `arrangrr_core` PUBLIC.
- **Rule 1:** the GUI target links **neither `arrangrr_core` nor `arrangrr_host`**, and `#include`s
  **zero** core headers. Model its `CMakeLists.txt` on `app/tools/arrstyle-converter` ("does not link
  arrangrr_core"), NOT on `arrangrr_host`.
- **Rule 2:** the GUI carries its **own** wire layer — a `LineBuffer`-style newline reassembly reader
  + a minimal JSON-line parser + its **own** name tables (section↔string, chord-quality↔suffix,
  warn↔string) written as plain strings. **Never** `static_cast<ChordQuality>` / `static_cast<
  SectionType>` a core enum. (`LineBuffer` is already dependency-free "by design" and *could* be
  extracted to a shared host-only header to avoid duplication — a file-placement call for Palladio;
  the extraction itself is safe. `arrstyle-converter/src/json.cpp` is an existing dependency-free
  JSON helper worth checking for reuse.)
- `note_names.{hpp,cpp}` is already a clean seam (takes only integers, no core includes) — reusable.

---

## 4. Coupled core prerequisite (already owner-approved) — `kChordFollowed`

Not a dependency, but sequenced with this phase: the approved additive `kChordFollowed` event
(§7.1 / §9.2 of `gui-ux-proposal.md`). Corelli's seam correction is folded there — four emit sites
in `engine.hpp` via one `Engine::emit_chord_followed` helper, a new no-heap `Producer
m_pending_source` in `FollowedContext`, `observe_chord_input` grows a `sink` param; 16 bytes
suffice; pinned by `test_abi_frozen` + a golden. Dependency-free core work — implementable
independently of the toolkit approval.

---

## 5. Reconciliation owed (on next merge)

`docs/DESIGN.md` §22 still calls the GUI tech stack "OPEN / NOT picked here" in three places
(≈ lines 932-934, 1047, 1136-1138). That text is **stale** vs D38. Reconcile it to "Dear ImGui,
locked by D38; backend GLFW+OpenGL3 per 0800" when the milestone merges (roadmap-updated-on-merge).

---

## 6. What the owner is approving

1. **The dependency set in §1** (Dear ImGui vendored + GLFW3 + the two impl backends + system GL),
   under policy 0800 — this is the one new host dependency footprint.
2. Implicitly, the **GLFW-not-SDL/hello_imgui** recommendation (say if you'd prefer SDL3).

Everything else (integration shape, boundary rules, the `kChordFollowed` seam) is architecture the
implementor follows; only the dependency in §1 needs your yes/no.
