# Phase 26: ThemeKey Enum Arrays - Context

**Gathered:** 2026-03-02
**Status:** Ready for planning

<domain>
## Phase Boundary

Replace `Theme::colors`, `Theme::rgbs`, and `Theme::gradients` — three `unordered_map<string, T>` — with enum-indexed `std::array`. Same pattern used successfully in v1.0 for Config keys (35-132x speedup on critical paths). Zero user-facing changes.

</domain>

<decisions>
## Implementation Decisions

### Claude's Discretion

All decisions in this phase are technical implementation details — no user-facing behavior, UI, or output changes. Claude has full flexibility on:

- **Enum structure** — Single ThemeKey enum for all ~50 color keys, or separate ColorKey + GradientKey enums. Consider that gradients are derived from color triplets (e.g., cpu_start/cpu_mid/cpu_end → gradient named "cpu").
- **Accessor API** — Whether to keep `c()`, `g()`, `dec()` function names or rename. Parameter type changes from `string` to enum.
- **Default/TTY theme representation** — Currently `unordered_map<string, string>` constants. May become constexpr arrays or stay as parse-time maps.
- **Theme file parsing** — String-to-enum mapping for parsing `.theme` files. Theme file format itself is NOT changed.
- **Fallback handling** — Current code uses `contains()` checks for optional colors (meter_bg, process_start, graph_text). Array approach needs equivalent fallback logic.
- **Migration strategy** — Big-bang or incremental across the 235 call sites in btop_draw.cpp (183), btop_menu.cpp (48), btop.cpp (4).

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches. Follow the proven BoolKey/IntKey/StringKey pattern from btop_config.hpp.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `btop_config.hpp` enum pattern: `BoolKey`, `IntKey`, `StringKey` with `COUNT` sentinel — direct template for ThemeKey
- `generateColors()` in btop_theme.cpp: builds colors/rgbs from source map, then derives gradients — central migration point
- `generateTTYColors()`: separate code path for TTY mode — needs same enum treatment

### Established Patterns
- Enum + `std::array` indexing used throughout v1.0 optimizations — well-tested approach
- Accessor functions (`Config::getB()`, `Config::getI()`, `Config::getS()`) return by const reference — Theme::c/g/dec follow same pattern
- `using` declarations at namespace scope (`using std::array`, `using std::string`) — consistent style

### Integration Points
- `Theme::c()` called 183 times in btop_draw.cpp — largest consumer
- `Theme::g()` and `Theme::dec()` used for gradient rendering and RGB math
- `Theme::setTheme()` is the entry point that populates all three maps
- Theme files parsed via string key matching — needs string→enum lookup table

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 26-themekey-enum-arrays*
*Context gathered: 2026-03-02*
