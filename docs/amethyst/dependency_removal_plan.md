# Dependency removal plan: spdlog + glm

Goal: drop the remaining heavy third-party deps in favor of small in-house equivalents, continuing the
direction set by removing toml++ (themes now use the hand-written `.ams` parser; layout config uses a
hand-written line format). Remaining fetched deps in `libamethyst/CMakeLists.txt`: `freetype`, `tracy`,
`lunasvg`. This plan covered **glm** and **spdlog** — both are now done.

## spdlog — DONE

Replaced with a tiny in-house logger in `src/logging/log.h` / `src/logging/log.cpp`:
- `enum class LogLevel { TRACE, DEBUG, INFO, WARN, ERR, CRITICAL }`
- Colorized stderr (ANSI, only when `isatty`), rotating file sink at 5 MB, recent-log buffer
- `AM_LOG_*` macros unchanged; `fmt::format` → `std::format`
- `am_assert.h` uses `fprintf(stderr, ...)` directly
- `spdlog` removed from `FetchContent_Declare`, `FetchContent_MakeAvailable`, `target_link_libraries`

## glm — DONE

Replaced with in-house math library in `src/math/math.h` / `src/math/math.cpp`:
- `vec2`, `vec3`, `vec4` — plain structs with `.r/.g/.b/.a` aliases via anonymous unions
- `mat3`, `mat4` — column-major storage matching GLM/GPU memory layout
- Full operator set, free functions (`dot`, `cross`, `length`, `normalize`, `abs`, `min`, `max`,
  `clamp`, `pow`, `fract`, `radians`, `pi<T>()`)
- `inverse(mat4)` — cofactor/adjugate method
- `packHalf1x16` / `packHalf2x16` — IEEE 754 binary16 conversion for GPU vertex packing
- Template conversion constructors and operators for implicit interop with GLM or any
  compatible type (engine-side can keep using GLM and pass types directly)
- All 21 internal files converted (`glm::` prefix stripped, includes swapped); backends and
  testapp also converted
- `glm` removed from `FetchContent_Declare`, `FetchContent_MakeAvailable`, `target_link_libraries`

## Remaining deps (staying)

- `freetype` — font rasterization
- `lunasvg` — SVG rasterization
- `tracy` — profiling (debug builds only)
