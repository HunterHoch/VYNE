# VYNE

Independent FiveM / GTA V Legacy Rockstar Editor export tools.

Phase 1 modernizes the existing export screen: true temporary render resolution and restoration, FPS, bitrate, format/codec, SDR color and output directory. Later camera, phone controller, optics and After Effects concepts remain in Talos.

## Current state

Version 0.3.2 adds deferred folder-picker completion redraws, a compact folder-name value with the full path in linked help, and disables check dimensions in Native export mode. This build awaits installation while FiveM is running.

Original Windows x64 diagnostic ASI. Automatic loading in FiveM build 3258 is confirmed by the installed plugin's diagnostic log. FiveM uses LoadLibrary and requires an `FX_ASI_BUILD` resource; initialization therefore runs on a short worker after the loader lock is released. The current resource declares build 3258 only.

Version 0.3.1 targets the native Export menu. See [native menu controls](docs/native-export-menu.md). **Output location** opens a folder picker; **VYNE export → Native export** uses Rockstar's encoder and copies its finished MP4 to that folder, preserving the gallery original. **Resolution check** instead tests the selected mode and restoration without creating media. Custom capture, replay cadence, encoding and audio remain unfinished. The user owns in-game testing; do not drive the game or run runtime experiments unless requested.

The 0.3.0 log confirmed ASI loading but hook activation failed, leaving the screen stock. Version 0.3.1 corrects native option field offsets and adds bounded hook retries with precise errors. Local checks pass; menu appearance and export delivery through the game still require the user's validation.

The earlier game-script/render-command probe was removed after a reported clip-load crash. The user confirmed the same crash with VYNE removed, then recorded a new clip and saved a working project, VYNE_Test. The exact old-clip failure remains unresolved. With FiveM closed, the new 0.2.0 probe was installed and explicitly enabled for the user's first mode-switch test. No mode-switch success is claimed yet.

## Build

With a Windows x64 C++ compiler and Windows SDK, initialize the pinned MinHook submodule before building:

```powershell
git submodule update --init
cmake -S . -B build -A x64
cmake --build build --config Release
```

Output: `build/Release/VYNE.asi`. Diagnostics write beside the binary under `VYNE/diagnostics.log`. This bootstrap is not installed automatically.

Alternatively, run `scripts\build.cmd` from an x64 Visual Studio Developer Command Prompt. This builds `build/VYNE.asi` and runs an isolated Windows loader/diagnostics smoke check. It requires no CMake installation. The smoke check does not establish FiveM compatibility.

## Plan

Talos workspace: Hoch Interactive. Plan: **VYNE — Rockstar Editor & Cinematic Tools** (`205deb37-ac7a-493b-89cc-3e3e7fd80095`). Canonical specification: `18fa536a-4db0-4e1e-ab21-936e220fd7b8`, revision 2. Preserve its 12 tasks, four milestones and separate Concept phase.

First gate: loading has been observed on build 3258; measure actual render targets, then implement and demonstrate reversible 2560×1440 → 3840×2160 → original mode. Compilation alone does not pass this gate.

No REPlus source, history or dependencies. No game binaries, extracted assets or proprietary source belong in this repository.

FiveM's documented plugin mechanism: https://docs.fivem.net/docs/client-manual/#using-plugins
