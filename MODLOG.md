# VYNE development journal

## 2026-10-26 — Bootstrap

- New original source tree: D:\VYNE; private remote: https://github.com/HunterHoch/VYNE.
- Route: native x64 ASI, diagnostics first; validate loader entry point before installation.
- Recovered and renamed existing Talos plan, preserving specification revision 2, all 12 tasks, dependencies, four milestones and Concept history.
- Read-only process inventory: no FiveM/GTA5 process observed. Runtime game build, render API, editor availability and render-target dimensions remain unverified.
- Plugin folder inventory: ScriptHookV.dll, VehicleMeshExporter.asi and its supporting files. Nothing changed there.
- VS Build Tools 2026 is registered but incomplete; discover usable compiler/SDK before claiming a build.
- Next: compile diagnostic bootstrap, validate in an isolated Windows host, then verify actual FiveM loader initialization and game build. No hooks or resolution switching until measured integration evidence exists.

### Verified starter checkpoint

- Despite incomplete VS installer registration, MSVC 14.50.35717 and Windows SDK 10.0.26100.0 work through vcvars64.bat.
- `scripts/build.cmd` built the original x64 ASI using /W4 /WX and static CRT. No third-party dependencies.
- Isolated Windows host passed LoadLibrary, exported diagnostics execution, log creation, once-only initialization and FreeLibrary. This is not FiveM loading proof.
- No plugin installation, game launch, graphics setting changes or export hooks performed. FiveM must be tested next; InitializeASI invocation is explicitly unverified.

## 2026-10-26 — Actual loader integration and user test handoff

- Examined official citizenfx/fivem `asi-five/src/Component.cpp`: FiveM requires a build-specific FX_ASI_BUILD resource and uses LoadLibrary without invoking InitializeASI. Added build 3258 resource and automatic deferred diagnostics. Windows smoke host also checks automatic startup and resource presence.
- Added read-only game-script/render-command diagnostics using installed FiveM exports. No render-thread TLS calls from the worker. Active renderer and target dimensions still need runtime evidence.
- Build and isolated Windows smoke check passed after these changes. Installed only VYNE.asi into the existing FiveM plugins directory; other plugins untouched. Installed SHA256: CB834BD457983E6F82C0088376919A1C38DAED7A572874C1D4654A71392EC7CA.
- Backup before launch: D:\VYNE\build\backup-20261026 (GTA settings.xml, Profiles, CitizenFX.ini). Original plugin hashes: build/plugins-before.json. Did not change game settings or existing plugin files.
- Actual automatic ASI load observed: plugin VYNE/diagnostics.log, 2026-10-26 19:26:11 UTC, PID 32880, FiveM_b3258_GTAProcess.exe, host version 1.0.3258.0. File version alone is not a render/build signature guard. No runtime.log observed before testing handoff.
- Official graphics source `rage-graphics-five/src/RenderHooks.cpp` exposes a VideoModeInfo layout and wraps game-owned mode/reset paths. These are research leads, not validated VYNE entry points; no signatures copied into production and no mode-change calls made.
- User said "Maybe let me handle the testing side of it": stopped game input and runtime experiments immediately. Leave in-game validation to the user. No 4K switch implemented or proven. No export hooks or encoding implemented.
- Next development: independently map and guard the current-build game-owned video-mode path, then implement snapshot/change/restore. Give the user a concrete testable build when ready; do not claim the resolution gate complete.

### Clip-load crash investigation

- User reports immediate crash when loading a clip. Latest two dumps show the same build-3258 signature: GTA5_b3258.exe+133243 (monkey-chicken-cola), while Scaleform says "Preparing clip".
- Latest dump 7f12472b-a6fe-464c-8245-7ac32fa9d726.dmp: EXCEPTION_ACCESS_VIOLATION write to address 0. Instruction is `mov [rcx], rax`, RCX=0. Captured stack entries are GTA, not VYNE. This does not independently exclude earlier corruption or establish root cause.
- VYNE startup log confirms loading; no render runtime log was generated. GPU diagnostic callback execution is unproven. Excluded script/render callback from both build paths pending lifecycle validation.
- After verifying no GTAProcess was running, moved only installed VYNE.asi to D:\VYNE\build\VYNE-render-probe-20261026.asi.disabled. No VYNE ASI now active in plugins; user can compare the same clip in the native editor. No other plugins/settings changed.
- Continue static development while user handles runtime comparison. Do not assume a clip/build mismatch or a VYNE fault without comparison evidence.

### Independent mode mapping and opt-in 0.2.0 probe

- User confirmed the same crash with VYNE removed and is recording a fresh clip. Continue development; no agent-driven in-game testing.
- Read-only code inspection of the user's mapped FiveM build3258 found one mode-routine match. No game state modified by inspection; copied code bytes are outside the repository at D:\VYNE-research and must never be committed.
- Independently checked the six native dword inputs at offsets 0..20, mode entry RVA 0x13A01C0, queued native mode RVA 0x1E81060, and swapchain reference RVA 0x13A02DD. Entry queues changes when thread ownership differs; false return does not necessarily mean rejection. The adapter checks the queued record and actual buffer dimensions separately.
- Added a default-off native probe, guarded by exact build/body and expected Present owner, preserving the six native fields except requested dimensions. Present interception runs the probe on the rendering thread. F6 requests 4K, F7/focus loss cancels, five seconds of ready state triggers restoration. Only an existing windowed native mode is accepted; no desktop/driver change APIs used.
- Added switch/restore session checks for success, distinct frame stabilization, cancellation while switching, partial failures, timeouts, resize during capture and incomplete restoration. All passed under MSVC /W4 /WX. The native adapter itself is NOT runtime verified and remains uninstalled/default-off.
- Recovery marker records original native mode before any request. A failed or crashed session refuses re-arming until reviewed; automatic cross-process recovery is not implemented.
- Next user evidence: fresh clip baseline; then opt-in mode probe. Next development after feasibility: identify clean pre-UI engine scene target, map native Export controls and implement capture/timebase/encoding in dependency order.

### User baseline now ready

- User recorded a new clip and saved a Rockstar Editor project named VYNE_Test with camera markers. New clip/project no longer crashes without VYNE. The earlier failure was reproducible without VYNE and is associated with the old clip; do not claim the old clip's exact corruption/build cause is known.
- VYNE_Test is the intended export acceptance fixture. Keep it intact. Agent does not drive the editor; user handles testing.
- Native Export is still stock. The 0.2.0 probe tests render-mode feasibility only and is not wired to Export; no misleading export-ready claim.

- After observing no FiveM GTAProcess running, installed 0.2.0 via scripts/Install-TestBuild.ps1 -EnableModeProbe. Source and installed SHA256 both FA422C7310F642F1B53502C4B3F64B4EF35583FC9C5A9B1AAAFDC483C2FD1E52. Only VYNE.asi and VYNE/VYNE.ini installed; previous experimental binary remains quarantined. User instructed to reopen VYNE_Test, press F6 once, observe five-second 4K request/restore, F7 to cancel. No agent-driven runtime test performed.
