# VYNE

Independent FiveM / GTA V Legacy Rockstar Editor export tools.

Phase 1 modernizes the existing export screen: true temporary render resolution and restoration, FPS, bitrate, format/codec, SDR color and output directory. Later camera, phone controller, optics and After Effects concepts remain in Talos.

## Current state

Original Windows x64 diagnostic ASI bootstrap. No game hooks, export interception, mode changes or encoding yet. `InitializeASI` / `VYNE_RunDiagnostics` explicitly run diagnostics outside DllMain; automatic invocation by FiveM has **not** been verified. Diagnostics record host path/version and loaded graphics modules, without treating these as proof of game build, active renderer or render dimensions.

## Build

With a Windows x64 C++ compiler and Windows SDK:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

Output: `build/Release/VYNE.asi`. Diagnostics write beside the binary under `VYNE/diagnostics.log`. This bootstrap is not installed automatically.

Alternatively, run `scripts\build.cmd` from an x64 Visual Studio Developer Command Prompt. This builds `build/VYNE.asi` and runs an isolated Windows loader/diagnostics smoke check. It requires no CMake installation. The smoke check does not establish FiveM compatibility.

## Plan

Talos workspace: Hoch Interactive. Plan: **VYNE — Rockstar Editor & Cinematic Tools** (`205deb37-ac7a-493b-89cc-3e3e7fd80095`). Canonical specification: `18fa536a-4db0-4e1e-ab21-936e220fd7b8`, revision 2. Preserve its 12 tasks, four milestones and separate Concept phase.

First gate: prove loading in a permitted FiveM session, measure actual render targets, then demonstrate reversible 2560×1440 → 3840×2160 → original mode. Compilation alone does not pass this gate.

No REPlus source, history or dependencies. No game binaries, extracted assets or proprietary source belong in this repository.

FiveM's documented plugin mechanism: https://docs.fivem.net/docs/client-manual/#using-plugins
