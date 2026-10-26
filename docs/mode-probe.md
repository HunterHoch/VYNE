# VYNE 0.2.0 mode probe

This is the first resolution feasibility build, not an exporter. In-game verification belongs to the user. VYNE remains removed from the active plugins folder while the user compares native clip loading.

## Build and install when the clean clip is ready

Build with `scripts\build.cmd` in an x64 VS Developer Command Prompt. Close FiveM, then run:

```powershell
& D:\VYNE\scripts\Install-TestBuild.ps1 -EnableModeProbe
```

This backs up any existing VYNE binary/config, copies only VYNE.asi and its configuration, and refuses installation while the game is running. No launcher or game input is automated. Without the explicit flag, a new configuration defaults to diagnostics only.

Initial supported runtime: **FiveM Legacy build 3258**, existing windowed/borderless DX11 swapchain. Other builds, fullscreen mode, mismatched code, missing swapchains, unexpected Present owners and unresolved recovery records are refused.

## User check

1. Open the working test clip at the normal resolution.
2. With the game focused, press **F6 once**. The probe requests 3840×2160 through the game-owned video-mode entry point. It waits for three distinct matching backbuffer frames, holds for five seconds, then requests the saved original mode.
3. **F7** cancels; losing focus also cancels. Check that the original appearance/resolution returns.
4. Logs: `%LOCALAPPDATA%\FiveM\FiveM.app\plugins\VYNE\mode-probe.log`. `requested=...`, `measured_backbuffer=...` and `restore_verified=...` are separate events. If no `armed` entry appears, report the refusal message.

The session stops on mismatched dimensions and times out after ten seconds. Failed restoration leaves `mode-recovery.txt`, disables another attempt in that process, and refuses re-arming on the next launch. Inspect/restore the original graphics state before moving that marker aside. A hard crash can prevent immediate rollback; the marker records the original values, it does not itself perform crash recovery.

## What this does and does not prove

- The tested state machine covers success, cancellation while switching, partial request failure, dimension timeout, dimension drift, restoration failure, and full-state verification. A repeated observation of one frame cannot satisfy the stabilization gate.
- The build-specific adapter uses a separately checked 24-byte native mode record. It changes width and height, preserves the remaining native fields, and calls the existing FiveM-wrapped game routine. It does not call Windows display-mode APIs or alter GPU driver settings. Baseline native fullscreen must be zero.
- A real 3840×2160 backbuffer proves its storage dimensions. It does **not** prove the scene before UI/downsampling was rendered at that resolution. Clean scene-target identification, capture, actual Export UI integration, replay timing, audio and encoding are still outstanding.
- The new probe is default-off and has not been exercised in-game. A successful C++ build is not evidence of successful mode switching.

## Mapping evidence

Official orientation sources: FiveM `asi-five/src/Component.cpp`, `rage-graphics-five/src/RenderHooks.cpp`, and installed DLL exports. Production code is independently authored; no upstream implementation or mod pattern table was imported.

Independent read-only review of the mapped local build3258 executable found one matching mode routine, with native input reads at +0,+4,+8,+12,+16,+20. The entry is RVA `0x13A01C0`; an untouched body fingerprint is validated at `0x13A0211`. The queued mode record resolves to RVA `0x1E81060`; the swapchain reference instruction is at `0x13A02DD`. Function invocation, thread assumptions, state restoration and interaction with actual clip playback remain runtime acceptance gates. Raw executable bytes are kept outside the repository at D:\VYNE-research and are never committed.

Public references:

- https://github.com/citizenfx/fivem/blob/master/code/components/asi-five/src/Component.cpp
- https://github.com/citizenfx/fivem/blob/master/code/components/rage-graphics-five/src/RenderHooks.cpp
