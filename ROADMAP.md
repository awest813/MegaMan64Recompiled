# Roadmap

This document tracks known issues, planned improvements, and future features for **Mega Man 64: Recompiled**.

Items are grouped by priority (High -> Low) and category.

---

## Recently Fixed (Audit 2026-03-27)

The following issues were identified and fixed during a comprehensive codebase audit:

| Severity | File | Fix |
|----------|------|-----|
| Critical | `src/main/main.cpp` | `SDL_Init()` check was `> 0` (never true); changed to `!= 0` so fatal SDL errors are caught. |
| Critical | `src/main/main.cpp` | `preload_executable()` returned `EXIT_FAILURE` (1/true) on error path; changed to `false`. |
| Critical | `src/game/input.cpp` | `controller_states` map modified from UI thread and read from game thread without a mutex. Wrapped all accesses in `cur_controllers_mutex`. |
| Critical | `src/game/rom_decompression.cpp` | `yaz0_decompress()` could read past the end of the input buffer on malformed data. Added bounds checks before every multi-byte read. |
| High | `src/main/main.cpp` | Window pointer used in `set_window_icon()`/`SDL_SetWindowFullscreen()` before null check. Moved null check before usage. |
| High | `src/game/config.cpp` | `getpwuid()` result dereferenced without null check on Linux. Added null guard. |
| High | `src/main/main.cpp` | `queue_samples()` did not validate `sample_count` was aligned to channel count. Added alignment check. |
| Medium | `src/main/main.cpp` | `OpenProcess` handle leaked on all code paths. Added `CloseHandle()` on success and failure paths. |
| Medium | `src/main/main.cpp` | Audio device and SDL never cleaned up on exit. Added `SDL_CloseAudioDevice()` and `SDL_Quit()`. |
| Low | `src/game/input.cpp` | `return false` used in functions returning `float`. Changed to `return 0.0f`. |
| High | `src/game/debug.cpp`, `src/game/scene_table.cpp`, `src/ui/ui_config.cpp` | Removed Majora's Mask placeholder warp data, guarded debug warps against invalid indices, and disabled the warp UI until Mega Man 64 destinations are mapped. |
| High | `src/game/debug.cpp` | `pending_set_time` now uses `0xFFFFFFFF` as the "no pending" sentinel so valid encoded times don't collide with the reset value. |

## Recently Fixed (Stability Pass 2026-03-30)

| Severity | File | Fix |
|----------|------|-----|
| High | `src/main/rt64_render_context.cpp` | `create_render_context()` only logged GPU init failure to stderr. Now also shows a user-facing message box via `zelda64::show_error_message_box()`. |
| Medium | `src/main/main.cpp` | `preload_executable()` was a no-op stub on Linux/macOS, printing a spurious "Failed to preload" warning. Now implemented via `mmap` + `mlock` with a `posix_madvise(MADV_WILLNEED)` fallback when locking privileges are unavailable. |
| Low | `src/game/quicksaving.cpp` | Removed two debug `printf` statements that leaked internal thread IDs to stdout in release builds. |
| High | `src/game/config.cpp` | Config load/save failures were only visible on stderr. User-facing message boxes now report failed default saves after load, full save failures from the UI, and missing config directory. |

---

## Phase 1: N64 Parity & Fidelity

Goal: Ensure the recompiled version is behaviorally identical to the original N64 game.

### 1A: Core Accuracy

| Priority | Item | Description |
|----------|------|-------------|
| High | Scene table accuracy | `src/game/scene_table.cpp` no longer ships Zelda placeholder data; populate it with Mega Man 64 stage/area data to re-enable debug warping. |
| ~~Medium~~ | ~~`recomp_powf` register access~~ | ~~`src/game/recomp_api.cpp:50` reads `ctx->f14.fl` directly instead of using `_arg<1, float>()`. Restore the commented-out accessor.~~ Fixed: already uses `_arg<1, float>()`. |
| Medium | RSP task handling | `get_rsp_microcode()` only handles `M_AUDTASK`. Verify no other RSP task types are used by MM64. |

### 1B: Audio Parity

| Priority | Item | Description |
|----------|------|-------------|
| High | Audio resampling quality | The `SDL_AudioCVT`-based resampler runs synchronously. Profile vs N64 output for fidelity; consider a higher-quality resampler if artifacts are audible. |
| Medium | Volume ramping | Verify rumble and volume curves match N64 behavior. |

### 1C: Save System

| Priority | Item | Description |
|----------|------|-------------|
| High | Flashram save/load | Verify existing Flashram save/load path works correctly for all MM64 save slots. |
| High | Quicksave re-enable | Fixed: removed `#if 0` gate, added main-thread context save/load, and guarded load against missing save state. |
| Medium | Save migration | Ensure saves from original N64 hardware can be imported. |

---

## Phase 2: Modern Graphics Enhancements

Goal: Leverage RT64 and native hardware for visually improved rendering while remaining faithful to the original art style.

### 2A: RT64 Integration

| Priority | Item | Description |
|----------|------|-------------|
| ~~High~~ | ~~Renderer error reporting~~ | ~~`create_render_context()` logs to stderr on failure but continues. Surface GPU driver errors to the user via message box.~~ Fixed: now shows a user-facing message box. |
| High | Internal resolution scaling | Expose a configurable internal resolution multiplier (2x, 4x, 8x) for sharper rendering on high-DPI displays. |
| Medium | MSAA / SSAA options | RT64 supports multi-sample anti-aliasing. Expose toggle and sample count in the graphics settings UI. |
| Medium | Dynamic resolution | Investigate RT64's frame-pacing support for consistent performance on variable hardware. |

### 2B: Texture & Visual Assets

| Priority | Item | Description |
|----------|------|-------------|
| High | Texture pack loader UI | All mods are auto-enabled at startup. Build a proper in-game texture pack browser with enable/disable, preview, and reload. |
| High | HD texture pack format | Document the `.rtz` mod container format and `rt64.json` content type API for community texture artists. |
| Medium | Community texture packs | Create example HD texture packs demonstrating the pack format (e.g., UI elements, environment textures). |
| Low | Sprite filtering options | Allow per-texture filtering mode (nearest vs linear) for preserving pixel art or smoothing 3D textures. |

### 2C: Shader Enhancements

| Priority | Item | Description |
|----------|------|-------------|
| Medium | Widescreen corrections | MM64 renders at 4:3. Fix HUD stretching, text positioning, and UI elements for widescreen aspect ratios. |
| Medium | Fog density tuning | `patches/graphics.h` contains commented-out fog reduction code. Expose fog density as a configurable option. |
| Low | Post-processing hooks | Investigate adding optional post-processing (bloom, color correction) via RT64's shader pipeline. |

---

## Phase 3: Quality of Life Improvements

Goal: Modernize the user experience without altering the game's core gameplay.

### 3A: Input & Controls

| Priority | Item | Description |
|----------|------|-------------|
| High | Mouse look | Mouse delta is computed in `input.cpp` but not hooked up to camera controls. Implement true mouse aim. |
| High | Controller hot-plug | Now thread-safe (C3 fix). Test and verify controller connect/disconnect during gameplay. |
| Medium | Adjustable deadzone | `// TODO adjustable deadzone threshold` in `input.cpp`. Expose radial deadzone slider in controls settings. |
| Medium | Input display overlay | Optional on-screen display showing current button/axis inputs for testing and streaming. |
| Low | Per-input-type sensitivity | Separate sensitivity sliders for mouse, gyro, and right-analog aiming. |

### 3B: Configuration & UI

| Priority | Item | Description |
|----------|------|-------------|
| ~~High~~ | ~~Config error surfacing~~ | ~~`load_config()`/`save_config()` failures are logged to stderr. Show critical config errors in the UI.~~ Fixed: batched dialogs via `zelda64::show_error_message_box()`. |
| Medium | Font atlas caching | `ui_renderer.cpp` rebuilds the font atlas on every UI reload. Cache to eliminate hitching. |
| ~~Medium~~ | ~~Config path caching~~ | ~~`get_app_folder_path()` calls `getenv()` on every invocation. Cache the result.~~ Fixed: result is cached via a static `bool cached` flag. |
| Medium | Controller binding UI | Allow re-binding all N64 buttons per-port with visual feedback. |
| Low | Multi-language support | Framework for community translation of UI strings. |

### 3C: Platform & Performance

| Priority | Item | Description |
|----------|------|-------------|
| High | Fullscreen toggle (Linux) | `SDL_SetWindowFullscreen` workaround in place. Remove once RT64 gains native fullscreen on Linux. |
| ~~Medium~~ | ~~Preload executable (non-Windows)~~ | ~~`preload_executable()` is a no-op on Linux/macOS. Implement via `mlock()` / `posix_madvise(MADV_WILLNEED)`.~~ Fixed: implemented via `mmap` + `mlock` with `posix_madvise(MADV_WILLNEED)` fallback. |
| Medium | `InputState` encapsulation | Refactor the large static struct in `input.cpp` into a proper class with documented thread-safety guarantees. |
| Low | macOS CI | Build is supported but less tested. Add macOS to CI via GitHub Actions. |

---

## Testing

| Priority | Area | Description |
|----------|------|-------------|
| High | `yaz0_decompress()` | Unit tests for valid data, truncated input, and malformed back-references (validate the new bounds checks). |
| High | Config serialisation | Round-trip save/load tests for each config section, including missing keys and corrupt JSON. |
| Medium | Input mapping | Unit tests for `assign_mapping()` / `get_input_binding()` with edge-case indices. |
| Medium | ROM validation | Unit tests for `decompress_mm()` with wrong size, wrong header, and correct input. |
| Medium | Save system | Integration tests for Flashram save/load with known save data. |
| Low | UI smoke tests | Investigate headless RmlUi rendering for automated UI regression testing. |

---

## Code Quality & Clean-up

| Priority | Item | Description |
|----------|------|-------------|
| Medium | Remove dead code | Clean up commented-out `REGISTER_FUNC` calls, unused `njpgdspMain` reference, and empty `register_patches.cpp`. |
| Medium | `ui_color_hack.cpp` vtable overwrite | `memcpy` over a `Rml::PropertyParserColour` vtable is fragile. Add a runtime layout check or find an upstream solution. |
| Medium | `from_bytes_le` unaligned access | `ui_renderer.cpp` uses `reinterpret_cast` on potentially unaligned data. Use `memcpy` for portability. |
| Low | `static_assert(false)` portability | `main.cpp:119` uses `static_assert(false && ...)` which may fail on strict C++11/14 modes. |
| Low | Rename `zelda64` namespace | The `zelda64` namespace is a template artifact. Rename to `mm64` or `megaman64` for project identity. |
