# VYNE

Independent FiveM / GTA V Legacy Rockstar Editor export tools.

Phase 1 modernizes the existing export screen: true temporary render resolution and restoration, FPS, bitrate, format/codec, SDR color and output directory. Later camera, phone controller, optics and After Effects concepts remain in Talos.

## Current state

Update: at the user's request,0.4.2 is now installed with matching source/binary hashes. It defers graphics initialization until the Export menu opens. Settings remain Render export,3840x2160,D:\VYNE\Output. Startup and4K output still await user validation; quarantine statements below describe the preceding recovery step.

User reported 0.4.1 freezing FiveM at launch. The installed ASI has been quarantined outside the plugins folder; VYNE is currently disabled. Version 0.4.2 defers graphics-device access and hook setup until the native Export menu has completed its first build, and adds initialization-stage logging. It is built locally and deliberately remains uninstalled. Exact hang mechanism and runtime recovery remain unverified.

Version 0.4.1 corrects the additional monitor-size clamp in the temporary window-mode path and adds live wait/cancellation diagnostics. Built, locally checked and installed after the user closed FiveM. GTA 4K output remains unverified.

Version 0.4.0 adds **Render export**: selected game mode, session-scoped bypass of GTA's native 1080p clamp, GPU/encoder dimension checks and restoration around the native bake pipeline. See [resolution export](docs/resolution-export.md). Native audio, timing and presentation remain engine-owned. Windows H.264 encoding at 3840×2160 passes locally; actual GTA 4K export and restoration still require the user's test. The compact folder display and completion redraw fix are included.

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
