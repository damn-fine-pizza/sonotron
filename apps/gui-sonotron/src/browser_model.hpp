#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Pure-data model for the Browser zone (ux-workstation.md §4.3): a
// searchable tree of the three draggable material kinds — Styles, Clips,
// MIDI seqs. No ImGui, no I/O; browser_panel.cpp is the only file that
// touches ImGui or drag-drop payloads (the *_panel/*_model split,
// docs/design/gui-fase2-mechanical-plan.md's Invariants).

namespace sonotron {

// The 16 builtin styles, in the SAME order as `style load <name>`
// (components/platform/hostrt/shell_music_commands.cpp::cmd_style's "load" verb
// resolves by name via `find_builtin_style`, case-insensitively) and as
// declared in components/core/arrangrr/include/arrangrr/arranger/style.hpp's
// `styles::kBuiltins` — read (read-only) to source this list; gui-sonotron
// never includes the core (D38, pure client), so this is a hand-copied
// literal of what was read, not an invented name list.
inline constexpr std::array<std::string_view, 16> kBuiltinStyleNames = {
    "basic", "pop",   "rock",   "ballad",  "funk",  "disco",   "house", "swing",
    "bossa", "samba", "reggae", "country", "blues", "shuffle", "latin", "motown",
};

// ImGui drag-drop payload id carrying a style index (std::size_t into
// kBuiltinStyleNames) from browser_panel.cpp's drag source to
// grid_panel.cpp's cell drop target. Declared here (a plain string literal,
// no ImGui dependency) so both panels share the exact same id without
// either including the other's header.
inline constexpr const char* kStyleDragPayloadId = "SONOTRON_STYLE_INDEX";

// ImGui drag-drop payload id carrying a SectionType byte (repeat-zone-real-
// contract.md SLICE 4a) from browser_panel.cpp's "variations" list drag
// source to grid_panel.cpp's scene-header drop target. Distinct from
// kStyleDragPayloadId above (a style INDEX) so ImGui::AcceptDragDropPayload
// never confuses the two payload shapes at a shared drop site.
inline constexpr const char* kVariationDragPayloadId = "SONOTRON_VARIATION_SECTION";

// Outer category selector (browser-redesign-taxonomy.md Phase 1, decision
// fork 1): which resource family(ies) the tree below renders. Reused via a
// wrapping toggle-label bar (browser_panel.cpp's render_category_toggles) --
// MULTIPLE categories can be visible simultaneously now, NOT a tab bar -- see
// that doc's §3 for why a literal ImGui::BeginTabBar does not fit 210px.
// Declaration order below is deliberately the SAME order the toggle bar lists
// entries in (browser_panel.cpp's local kCategoryOrder mirrors it). Do NOT
// add Songs/Performances/Chords/Loops/Pad-FX-Groove/Controller-maps/Routing
// here -- the taxonomy doc stages every one of those into a later phase or
// cuts them outright; adding one here without a new owner-approved phase
// would silently widen this slice's scope.
enum class BrowserCategory : std::uint8_t {
  kStyles,
  kVariations,
  kVoices,
  kKits,
  kClips,
};

inline constexpr std::size_t kBrowserCategoryCount = 5;

// Display label for a category (used by the combo and by each category's own
// section header). Never returns an empty view.
std::string_view browser_category_label(BrowserCategory category);

// The 128 canonical General MIDI program names, program 0..127, in order --
// a hand-copied literal of components/platform/hostrt/gm_program.cpp's own
// kGmNames array (read as reference only; gui-sonotron never #includes a
// components/ header, D38, the SAME discipline kBuiltinStyleNames above
// already documents). Index-parallel with the source array, so sending this
// exact string round-trips through arrangrr::host::parse_gm_program
// unchanged (case/spacing-insensitive match there).
inline constexpr std::array<std::string_view, 128> kGmVoiceNames = {
    "Acoustic Grand Piano",
    "Bright Acoustic Piano",
    "Electric Grand Piano",
    "Honky-tonk Piano",
    "Electric Piano 1",
    "Electric Piano 2",
    "Harpsichord",
    "Clavi",
    "Celesta",
    "Glockenspiel",
    "Music Box",
    "Vibraphone",
    "Marimba",
    "Xylophone",
    "Tubular Bells",
    "Dulcimer",
    "Drawbar Organ",
    "Percussive Organ",
    "Rock Organ",
    "Church Organ",
    "Reed Organ",
    "Accordion",
    "Harmonica",
    "Tango Accordion",
    "Acoustic Guitar (nylon)",
    "Acoustic Guitar (steel)",
    "Electric Guitar (jazz)",
    "Electric Guitar (clean)",
    "Electric Guitar (muted)",
    "Overdriven Guitar",
    "Distortion Guitar",
    "Guitar harmonics",
    "Acoustic Bass",
    "Electric Bass (finger)",
    "Electric Bass (pick)",
    "Fretless Bass",
    "Slap Bass 1",
    "Slap Bass 2",
    "Synth Bass 1",
    "Synth Bass 2",
    "Violin",
    "Viola",
    "Cello",
    "Contrabass",
    "Tremolo Strings",
    "Pizzicato Strings",
    "Orchestral Harp",
    "Timpani",
    "String Ensemble 1",
    "String Ensemble 2",
    "SynthStrings 1",
    "SynthStrings 2",
    "Choir Aahs",
    "Voice Oohs",
    "Synth Voice",
    "Orchestra Hit",
    "Trumpet",
    "Trombone",
    "Tuba",
    "Muted Trumpet",
    "French Horn",
    "Brass Section",
    "SynthBrass 1",
    "SynthBrass 2",
    "Soprano Sax",
    "Alto Sax",
    "Tenor Sax",
    "Baritone Sax",
    "Oboe",
    "English Horn",
    "Bassoon",
    "Clarinet",
    "Piccolo",
    "Flute",
    "Recorder",
    "Pan Flute",
    "Blown Bottle",
    "Shakuhachi",
    "Whistle",
    "Ocarina",
    "Lead 1 (square)",
    "Lead 2 (sawtooth)",
    "Lead 3 (calliope)",
    "Lead 4 (chiff)",
    "Lead 5 (charang)",
    "Lead 6 (voice)",
    "Lead 7 (fifths)",
    "Lead 8 (bass + lead)",
    "Pad 1 (new age)",
    "Pad 2 (warm)",
    "Pad 3 (polysynth)",
    "Pad 4 (choir)",
    "Pad 5 (bowed)",
    "Pad 6 (metallic)",
    "Pad 7 (halo)",
    "Pad 8 (sweep)",
    "FX 1 (rain)",
    "FX 2 (soundtrack)",
    "FX 3 (crystal)",
    "FX 4 (atmosphere)",
    "FX 5 (brightness)",
    "FX 6 (goblins)",
    "FX 7 (echoes)",
    "FX 8 (sci-fi)",
    "Sitar",
    "Banjo",
    "Shamisen",
    "Koto",
    "Kalimba",
    "Bag pipe",
    "Fiddle",
    "Shanai",
    "Tinkle Bell",
    "Agogo",
    "Steel Drums",
    "Woodblock",
    "Taiko Drum",
    "Melodic Tom",
    "Synth Drum",
    "Reverse Cymbal",
    "Guitar Fret Noise",
    "Breath Noise",
    "Seashore",
    "Bird Tweet",
    "Telephone Ring",
    "Helicopter",
    "Applause",
    "Gunshot",
};

// The 9 canonical General MIDI Level 2 percussion-kit names, in ASCENDING
// program-number order -- a hand-copied literal of components/platform/
// hostrt/gm_program.cpp's own kGmDrumKits array (read as reference only;
// gui-sonotron never #includes a components/ header, D38, the SAME
// discipline kGmVoiceNames above already documents). Index-parallel with
// kGmDrumKitPrograms below.
inline constexpr std::array<std::string_view, 9> kGmDrumKitNames = {
    "Standard Kit", "Room Kit",  "Power Kit",     "Electronic Kit", "TR-808 Kit",
    "Jazz Kit",     "Brush Kit", "Orchestra Kit", "SFX Kit",
};

// The GM program number each kGmDrumKitNames entry sends -- index-parallel
// with kGmDrumKitNames above.
inline constexpr std::array<int, 9> kGmDrumKitPrograms = {
    0, 8, 16, 24, 25, 32, 40, 48, 56,
};

// Primary taxonomy axis for the style browser at scale (task #30, docs/
// proposals/style-browser-corpus-scale.md §2.1's 7+1-family tree, derived
// from the corpus's own measured rhythmic-family clustering, NOT from
// filename vibes). kOther is not decorative: a real fraction of any future
// imported corpus will have no confident family (the doc measured 54% of
// the real 1010-style corpus carrying no genre token at all) -- showing
// that state honestly, as its own filterable bucket, is the difference
// between a taxonomy and a silent mis-bucketing a player will notice.
enum class StyleFamily : std::uint8_t {
  kPopRockBallad,
  kDanceFourOnFloor,
  kFunkGroove,
  kSwingShuffleJazz,
  kLatinClave,
  kBallroomTraditional,
  kWorldRegional,
  kOther,
};

// Display label for a family bucket (used by the browser panel's section
// headers and its filter combo). Never returns an empty view -- every
// enumerator has a real label.
//
// NAMING SUBTLETY (found while building this taxonomy, do not re-derive it):
// style_matches_filter's text-search haystack is `name + " " + style_family_
// label(family)` -- so a label that spells out a DIFFERENT sibling style's
// own name inside the SAME family silently breaks "family filter + text
// narrows correctly", because every style in that family shares the same
// label text. kPopRockBallad's siblings are pop/rock/ballad/country/motown;
// a label of "Pop / Rock / Ballad" would make searching "roc" match every
// one of them (their haystacks all carry the substring "Rock" via the
// label), not just "rock" itself -- hence "Pop / Ballad" below (rock is
// still found trivially by its own name). The same scrutiny was applied to
// every other family with more than one sibling among today's 16 built-ins:
// kSwingShuffleJazz's siblings are swing/blues/shuffle, so its label avoids
// spelling either "swing" or "shuffle" ("Big Band / Jazz" instead); kLatinClave's
// siblings are bossa/samba/reggae/latin, so its label avoids all four
// ("Clave / Tropical" instead). kFunkGroove/kOther have exactly one sibling
// each among the 16 (funk, basic) so there is nothing to cross-contaminate;
// kBallroomTraditional/kWorldRegional have none of the 16 assigned to them
// yet, so their labels use the doc's own family names verbatim.
std::string_view style_family_label(StyleFamily family);

// Hand-classified per docs/proposals/style-browser-corpus-scale.md §2.1's
// family table, applied to TODAY's 16 built-in styles (kBuiltinStyleNames,
// same order/index). "basic" is a generic default style authoring no
// distinctive rhythmic signature of its own -- it deliberately lands in
// kOther rather than being force-fit into a family it doesn't really
// belong to, exercising the fallback bucket with REAL data, not only a
// synthetic test case.
inline constexpr std::array<StyleFamily, 16> kBuiltinStyleFamilies = {{
    StyleFamily::kOther,             // basic
    StyleFamily::kPopRockBallad,     // pop
    StyleFamily::kPopRockBallad,     // rock
    StyleFamily::kPopRockBallad,     // ballad
    StyleFamily::kFunkGroove,        // funk
    StyleFamily::kDanceFourOnFloor,  // disco
    StyleFamily::kDanceFourOnFloor,  // house
    StyleFamily::kSwingShuffleJazz,  // swing
    StyleFamily::kLatinClave,        // bossa
    StyleFamily::kLatinClave,        // samba
    StyleFamily::kLatinClave,        // reggae
    StyleFamily::kPopRockBallad,     // country
    StyleFamily::kSwingShuffleJazz,  // blues
    StyleFamily::kSwingShuffleJazz,  // shuffle
    StyleFamily::kLatinClave,        // latin
    StyleFamily::kPopRockBallad,     // motown
}};

class BrowserModel {
 public:
  std::size_t style_count() const { return kBuiltinStyleNames.size(); }
  std::string_view style_name(std::size_t index) const;
  StyleFamily style_family(std::size_t index) const;

  // Clips / MIDI seqs: user-authored material (§4.3's "Clips" and
  // "MIDI seqs" branches). Nothing is authored yet in this slice — there is
  // no recorder/authoring UI and no clip primitive on the wire — so both
  // stay empty lists, an honest placeholder browser_panel.cpp renders as
  // "(none authored yet)" rather than inventing sample entries.
  const std::vector<std::string>& clip_names() const { return m_clips; }
  const std::vector<std::string>& midi_seq_names() const { return m_midi_seqs; }

  // The "[search…]" field (§3 wireframe): case-insensitive substring match
  // against `name + " " + style_family_label(family)` (task #30 broadens the
  // haystack beyond the name alone, per docs/proposals/style-browser-corpus-
  // scale.md §3.2). An empty filter matches everything. Per-category (§3:
  // "search is per-active-tab, never a global cross-family search") -- these
  // read/write whichever category is CURRENTLY active (see m_category).
  void set_search_filter(std::string filter);
  const std::string& search_filter() const;

  // Family filter (task #30): nullopt means "no family filter, every family
  // matches" -- ANDed with the text search above in style_matches_filter.
  void set_family_filter(std::optional<StyleFamily> family) { m_family_filter = family; }
  std::optional<StyleFamily> family_filter() const { return m_family_filter; }

  bool style_matches_filter(std::size_t index) const;

  std::size_t voice_count() const { return kGmVoiceNames.size(); }
  std::string_view voice_name(std::size_t index) const { return kGmVoiceNames[index]; }

  BrowserCategory category() const { return m_category; }
  void set_category(BrowserCategory category) { m_category = category; }

  // Toggle-bar visibility (browser-redesign-taxonomy.md Phase 1): whether a
  // given category's section renders inside the scrollable tree. Independent
  // of m_category (which now only tracks "last touched" for the bottom
  // search field, see filter_for below) -- multiple categories can be
  // visible at once. kStyles is visible by default, the rest start hidden.
  bool category_visible(BrowserCategory category) const {
    return m_category_visible[static_cast<std::size_t>(category)];
  }
  void set_category_visible(BrowserCategory category, bool visible) {
    m_category_visible[static_cast<std::size_t>(category)] = visible;
  }

  // Independent per-category filter accessor: unlike search_filter() (which
  // always reads/writes whichever category is CURRENTLY active), this reads
  // a SPECIFIC category's own filter slot regardless of which one is active
  // -- needed once multiple sections can be visible simultaneously, so each
  // keeps its own text filter isolated from the others.
  const std::string& filter_for(BrowserCategory category) const {
    return m_filters[static_cast<std::size_t>(category)];
  }

  // Voice destination (`program <port>[:channel]`): local-only, no wire
  // readback (parts_model.hpp's own gm_program stays -1 for the identical
  // reason -- no per-part program readback exists on the wire today). Channel
  // is 1-based here, matching the CLI's own display convention; clamped to
  // [1,16]. Port defaults to "out0", the only realized/audible output port in
  // gui-sonotron today (Phase-6 Theme 2).
  std::string_view voice_port() const { return m_voice_port; }
  void set_voice_port(std::string port);
  int voice_channel() const { return m_voice_channel; }
  void set_voice_channel(int channel_one_based);

  // Client-side "last sent" echo (index into kGmVoiceNames, -1 = none) -- the
  // SAME class of local-only highlight UiState::active_style already is for
  // Styles, kept here instead since ui_state.hpp is out of scope for this
  // slice. NEVER presented as wire-confirmed.
  int last_voice_sent() const { return m_last_voice_sent; }
  void set_last_voice_sent(int index) { m_last_voice_sent = index; }

  // Builds the exact `program <port>[:ch] <voice>` wire line for a voice pick.
  // Pure string logic (no ImGui) so it is unit-testable without a headless
  // harness.
  std::string build_program_verb(std::string_view voice_name) const;

  std::size_t kit_count() const { return kGmDrumKitNames.size(); }
  std::string_view kit_name(std::size_t index) const { return kGmDrumKitNames[index]; }

  // Kit destination (`program <port>[:channel]`): independent of the Voices
  // tab's own destination (m_voice_port/m_voice_channel) because a kit send
  // defaults to the GM PERCUSSION channel (10), not channel 1 -- sending a
  // kit program change on a melodic channel would silently retune whatever
  // instrument already lives there instead of picking a drum kit. Channel is
  // 1-based here, matching the CLI's own display convention; clamped to
  // [1,16]. Port defaults to "out0", the only realized/audible output port in
  // gui-sonotron today (Phase-6 Theme 2).
  std::string_view kit_port() const { return m_kit_port; }
  void set_kit_port(std::string port);
  int kit_channel() const { return m_kit_channel; }
  void set_kit_channel(int channel_one_based);

  // Client-side "last sent" echo (index into kGmDrumKitNames, -1 = none) --
  // mirrors m_last_voice_sent's own local-only highlight discipline.
  int last_kit_sent() const { return m_last_kit_sent; }
  void set_last_kit_sent(int index) { m_last_kit_sent = index; }

  // Builds the exact `program <port>[:ch] <program-number>` wire line for a
  // kit pick. A BARE PROGRAM NUMBER (not the kit name) -- the host's
  // numeric fast path (components/platform/hostrt/gm_program.cpp's
  // parse_gm_program) already accepts 0..127 unambiguously, so this never
  // needs the kit names taught to the melodic-name matcher. Pure string
  // logic (no ImGui) so it is unit-testable without a headless harness.
  std::string build_kit_verb(std::size_t index) const;

 private:
  std::vector<std::string> m_clips;
  std::vector<std::string> m_midi_seqs;
  std::array<std::string, kBrowserCategoryCount> m_filters;
  std::optional<StyleFamily> m_family_filter;
  BrowserCategory m_category = BrowserCategory::kStyles;
  std::array<bool, kBrowserCategoryCount> m_category_visible = {true, false, false, false, false};
  std::string m_voice_port = "out0";
  int m_voice_channel = 1;
  int m_last_voice_sent = -1;
  std::string m_kit_port = "out0";
  int m_kit_channel = 10;
  int m_last_kit_sent = -1;
};

}  // namespace sonotron
