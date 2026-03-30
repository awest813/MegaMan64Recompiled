# Mega Man 64: Recompiled

A native PC recompilation of **Mega Man 64** (N64) built with the [N64Recomp](https://github.com/Mr-Wiseguy/N64Recomp) toolchain.

![Mega Man 64 Recompiled screenshot](https://github.com/user-attachments/assets/6e5d6ca1-3677-4eaa-9fea-e056c56bb674)

## Current Status

This project is currently in **alpha** and under active development.

## What Works Today

- Native builds for **Windows**, **Linux**, and **macOS**
- RT64 rendering backend (**Vulkan / D3D12 / Metal**)
- Configurable graphics settings:
  - Resolution
  - Aspect ratio (Original / Expand / Manual)
  - Anti-aliasing (None / MSAA 2x / 4x / 8x)
  - Refresh rate (Original / Display / Manual)
- Keyboard and controller support through SDL2
- Controller features including **gyro** and **rumble**
- In-game configuration UI (graphics, controls, audio)
- Launcher UI with ROM selection / auto-detection
- Texture pack support via `.rtz` mod container format (auto-loaded)
- Standard Flashram save handling

## Known Limitations

- Widescreen/HUD parity improvements are still in progress
- Texture packs are auto-enabled; per-pack enable/disable UI is not implemented yet

## Requirements

- A legally obtained **NTSC-U Mega Man 64 (USA Rev 1)** ROM in **compressed `.z64` format**
- Recommended filename in repo root: `megaman64.us.z64` (see `us.rev1.toml`)

> [!IMPORTANT]
> Use the original compressed ROM file. A decompressed ROM is not supported for normal play.

## Building

See [BUILDING.md](BUILDING.md) for setup and build instructions.

## License

See [LICENSE](LICENSE) for licensing details.
