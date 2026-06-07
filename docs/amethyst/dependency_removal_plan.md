# Dependency removal plan: spdlog + glm

Goal: drop the remaining heavy third-party deps in favor of small in-house equivalents, continuing the
direction set by removing toml++ (themes now use the hand-written `.ams` parser; layout config uses a
hand-written line format). Remaining fetched deps in `libamethyst/CMakeLists.txt`: `glm`, `spdlog`,
`freetype`, `tracy`, `lunasvg`. This plan covers **glm** and **spdlog** only (freetype/lunasvg/tracy
stay — they do real, hard work).

Scope figures below are from a usage survey of `libamethyst/src`, `libamethyst/include`, `backends`
(excluding `vendor/`). Re-run the greps before starting; numbers will drift.

## spdlog (small, localized)

Used in only 3 files: `logging/log.h`, `logging/log.cpp`, `utils/am_assert.h`. Everything else goes
through the `AM_LOG_*` / `AM_ASSERT` macros, so the blast radius is the macro layer, not call sites.

spdlog also transitively pulls **fmt**. The log macros use `{}` placeholder formatting.

Approach:
- Replace the spdlog sink with a tiny logger (level enum, timestamp, level tag, ostream/`fwrite` to
  stderr; optional color). Keep the `AM_LOG_TRACE/DEBUG/INFO/WARN/ERROR/CRITICAL` macro names and
  signatures unchanged so no call site moves.
- Replace `{}` formatting with `std::format` (C++20, already in use elsewhere). The `{}` syntax is
  source-compatible. Watch: `std::format` wants a compile-time format string; for the variadic macro
  case use `std::vformat(fmt, std::make_format_args(...))` to accept runtime strings.
- Drop `spdlog` from `FetchContent_Declare`, `FetchContent_MakeAvailable`, and `target_link_libraries`.

Risks: minor. Mostly making sure every existing `AM_LOG_*("... {} ...", args)` call still formats. A
grep of `AM_LOG_` call sites is the test surface. No public API change (macros are internal).

## glm (bigger surface, conceptually small)

21 files include glm; ~600 symbol references, but the vocabulary is narrow. Inventory:

| Symbol | Count | Notes |
|--------|------:|-------|
| `glm::vec2` | 401 | the workhorse |
| `glm::vec3` | 108 | |
| `glm::vec4` | 56 | |
| `glm::dot` | 18 | |
| `glm::normalize` | 16 | |
| `glm::mat4` | 14 | transforms |
| `glm::cross` | 11 | |
| `glm::max` / `glm::min` | 10 / 8 | component-wise |
| `glm::length` | 7 | |
| `glm::clamp` | 7 | |
| `glm::mat3` | 3 | |
| `glm::packHalf2x16` | 3 | **half-float packing (rendering)** |
| `glm::pow` / `glm::inverse` / `glm::abs` | 2 each | `inverse` is mat inverse |
| `glm::radians` / `glm::pi` / `glm::fract` / `glm::packHalf1x16` | 1 each | |

What the in-house `math/` must provide:
- `vec2/vec3/vec4`: ctors, `+ - * /` (vec/scalar), `==`, swizzle-free member access (`.x/.y/.z/.w`,
  and `.r/.g/.b/.a` if used — check). Free functions `dot, cross, length, normalize, abs,
  min, max, clamp, pow, fract` (component-wise where glm is).
- `mat3/mat4`: storage + `mat * vec`, `mat * mat`, `inverse`, identity/translation/scale/rotation
  builders (check how transforms are currently built — likely translate/scale/rotate compose).
- scalars: `radians`, `pi` constant.
- **half-float**: `packHalf2x16` / `packHalf1x16` (IEEE 754 binary16 conversion). This is the one
  genuinely fiddly piece; port a known-correct float->half routine. Used by the renderer's vertex
  packing, so correctness is verifiable visually + by unit test.

Constraints / gotchas:
- glm is column-major; match its memory layout where data is handed to the GPU (vertex/uniform
  buffers) or the backend shaders break. Verify against the Vulkan backend's expectations.
- glm applies operations in a specific order for transform composition; mirror it so visuals are
  identical.
- Keep the type names (`using vec2 = ...`) so the 21 files need only an include swap, not a rewrite,
  if the in-house types are drop-in. Decide: shadow the `glm::` namespace/names or do a mechanical
  find-replace `glm::vec2` -> `am::vec2` etc.

Suggested order:
1. Build `math/` (vec2/3/4, then mat3/4, then the free functions, then half-packing) with unit tests
   (the test harness exists: `libamethyst/tests/*_test.cpp`, `-DAMETHYST_BUILD_TESTS=ON`).
2. Swap includes/usages file-by-file; keep glm linked until the last file is converted, then drop it
   from CMake.
3. Visual regression: run the demos and compare; the renderer (half-packing, mat layout) is where
   mistakes show up.

Risks: medium. The math itself is easy; the GPU-facing memory layout and half-float packing are where
silent corruption hides. Land it behind the test suite + a demo diff.

## Doing both

Independent; either order. spdlog is the quick win (few files, no GPU risk) — good warm-up. glm is the
larger, renderer-sensitive one. Neither touches the public builder/`.ams` API, so this can happen in
parallel with (or after) the engine migration.
