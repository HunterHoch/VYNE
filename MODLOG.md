# VYNE development journal

## 2026-10-26 — Export waiting at mode switch (0.4.1)

- User reports Export does not start. Installed logs show two selected-resolution requests from2560x1440 to3840x2160, no GPU-ready event, then cancellation/restoration roughly7-8seconds later. No encoder callback ran. FiveM logs record mode request success but no corresponding backbuffer rebuild. Do not attribute this to encoder failure or claim4K mode switched.
- Static tracing found a second monitor-size clamp in window rectangle fitting helper RVA0x1395A74. Mode request and frame-boundary resize validation both call it (return addresses0x13A0398/0x13A0865). It adjusts window chrome, then shrinks client dimensions to the monitor bounds. New guarded detour preserves selected temporary client dimensions for those callers only, including original dimensions during restore. Native resize/resource machinery still owns buffer recreation; no direct DXGI ResizeBuffers or desktop/driver changes.
- Added once-per-second actual GPU-size/focus observations, native request/queue outcomes and explicit cancellation reason. Editor help now redraws on mode status transitions on the UI thread. Prior logs cannot distinguish F7 from focus loss; new logs do. User owns runtime validation; no game control performed.
- 0.4.1 MSVC /W4 /WX build and all five local checks passed. Built SHA256 D3209B028C92223EB2F3A40748E8C54783746CFBB271C903A193F0C8C68AA019. NOT installed: FiveM_b3258_GTAProcess PID40224 is still running. Install using scripts/Install-TestBuild.ps1 after user closes it; leave current game session alone.

## 2026-10-26 — Selected-resolution native bake integration (0.4.0)

- Resumed after usage interruption: unfinished native_resolution.cpp, mode-session orchestration, MP4 dimension checks and platform encoder test were present but uncommitted. 0.3.2 folder-display fix had already been installed while FiveM was closed.
- Independently traced build3258 VideoEncode string reference to capture constructor RVA 0x325594; its caller gets capture dimensions through 0x324B1C and shared fit helper 0x31E3DC. The helper explicitly caps width at 1920 and height at 1080. Four callers feed capture and encoder configuration. Added guarded detours overriding only these callers inside a selected-resolution session; unrelated/native-stock calls remain unchanged.
- Render export requests selected game-owned windowed mode, waits for three distinct buffer frames, then dispatches native Export on the editor UI thread. An atomic dispatch claim prevents recursive UI callbacks starting another bake. GPU observation checks capture target, first-draw viewport and a same-resolution color source. A forwarding Media Foundation writer checks video input/output dimensions, forwards samples/audio/timestamps unchanged, and observes Finalize/errors/cancel. Original engine presentation and watermark behavior remain; this is not the independent clean-capture backend.
- Teardown requests native delayed close, releases held capture textures only after playback closes, and verifies original mode restoration. GPU dimension drift also closes playback before any reset. Only successful encoder finalization, GPU verification, mode restoration and matching closed MP4 track dimensions allow delivery. Wrong-size output is refused rather than relabeled/upscaled.
- MSVC /W4 /WX build, isolated loader, graphics rollback, row/settings and output checks pass. Platform Media Foundation test encoded four actual 3840x2160 NV12 frames; ffprobe reports H264 Constrained Baseline level5.1, 3840x2160, four frames. Actual 1080p native fixture is accepted as1080p and rejected as4K. This is local codec/model evidence only; GTA4K output, scene cleanliness and restoration need the user's runtime test.
- No game launched or driven; D:\REPlus and unrelated plugins untouched. Canonical true-resolution task remains incomplete until measured user output. Talos connector became unavailable during continuation; this journal is the durable handoff pending sync.
- Final 0.4.0 warnings-as-errors build and all five local checks passed after continuation fixes. Installed with FiveM closed; source/installed SHA256 BEB8D67251AB219BA68CBBEF0A20491BF6D2323F4D7DCE126D3D1C7B19484C58. Prior VYNE binary/config backed up. Export.ini preserved: 3840x2160, C:\Users\hunte\Downloads\test. No runtime 4K claim. Next user action: VYNE_Test > Export > Render export > 3840x2160 > Export; collect resolution-export.log, mode-probe.log and delivered MP4, including audio and original mode restoration.

## 2026-10-26 — Folder completion redraw and compact path (0.3.2)

- User confirmed folder picker and native output delivery to C:\Users\hunte\Downloads\test, but choosing text stayed cached until another native redraw. Subsequent screenshot showed full saved path overlapping the row label. Installed settings and output receipt independently confirm the selected folder and native delivery; menu log confirms seven injected rows and successful hook activation after 19 attempts.
- Folder worker now publishes a completion redraw request (including cancel/failure). Existing native selectability callback consumes it on the recorded menu UI thread, outside BuildMenu recursion. Local machine-code inspection confirms native focused input calls selectability at RVA 0x1CAE29 using active menu/column/row globals; no new hooks or worker-thread engine UI calls added.
- Output location value now shows the folder name only, shortened to 12 UTF-16 units without splitting surrogate pairs. Linked help carries the full saved path. Check resolution row is disabled and displays Game settings during Native export; selected 4K check dimensions never implied 4K encoding capability.
- MSVC /W4 /WX build and loader, rollback, row/settings/capability and output checks pass. User runtime validation of the new deferred redraw remains outstanding. FiveM is running, so 0.3.2 is built but not installed; loaded 0.3.1 remains intact.
- Actual 4K capture/encoding remains open per canonical specification sections 03/04: verified full-resolution clean scene target, replay timebase, encoder and restoration are required. Do not present native 1080p delivery as this capability.

## 2026-10-26 — Stock-screen feedback and output delivery (0.3.1)

- User screenshot showed only stock export rows. Installed diagnostics confirmed ASI loading; export-menu.log recorded hook activation failure and rollback, without a precise error status. Native menu integration remains unverified.
- Reviewed public REPlus row definitions and hook startup handling at user's request; D:\REPlus remains untouched. Corrected independently authored native option layout: Context +4, Block +8, Action +0x14, Warning +0x1C, DependentAction +0x20. Added static assertions and bounded MinHook snapshot/allocation-error retries with terminal status logging. Exact old activation failure is still unknown.
- Renamed folder row Output location and connected Native export to finished Rockstar MP4 delivery. Worker snapshots the native rendered folder before Export, waits for changed/stable files with no open writer and complete container boxes, then copies through a temporary file without overwriting destination clips. Native gallery originals are preserved; a receipt records the delivered path. Custom VYNE capture/encoding and 4K media remain unfinished.
- Found the existing native VYNE_Test.mp4 under LocalAppData/Rockstar Games/GTA V/videos/rendered: 1920x1080 H264, 29.97 FPS, AAC audio, 2.769436 seconds. This is native output, not proof of VYNE rendering.
- MSVC /W4 /WX build and loader, mode rollback, native-row/settings/retry and output tests passed. Output tests also copied the actual native VYNE_Test.mp4 into an ignored test directory and checked byte preservation. No game control or runtime experiment performed; user owns menu appearance, input and game-triggered output validation.
- Installed 0.3.1 with FiveM closed; source and installed SHA256 both 6CB1F955401BD9F106E20FA295F6C59868A6254DC012BA6A0EF772547C9A7D5C. Previous own binary/config backed up. Default native menu enabled; F6 probe disabled.

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

## 2026-10-26 — Native Export menu integration requested

- User rejected the standalone F6 workflow and explicitly requested native Export menu hooks similar to REPlus. Native menu integration is now the active deliverable; no dependency gate is treated as a reason to withhold this authorized implementation.
- Independently mapped the already captured build3258 executable: EXPORT_SETTINGS was located via a calculated Jenkins hash and menu lookup/caller analysis. Verified menu array RVA 0x201BF98, menu stride 0x38, option stride 0x28, options at +0x18, label/toggle/unique fields at +0/+0x18/+0x24, linked help at +0x0C, content block at menu +0x28. No game files enter Git.
- Native BuildMenu 0x15EFF8, AdjustToggle 0x15CC74, GetToggleString 0x1811E0, CText wrapper 0xDC05FC, IsItemSelectable 0x192B1C (also called by native IsItemEnabled), Export trigger 0x1B6A34, refresh 0x19E664, allocator/free 0x12E0/0x1318 were checked against local machine code. Guarded hooks use pinned MinHook v1.3.4; upstream license retained and installed. REPlus public source was architecture research only; no upstream routine or pattern table copied into VYNE.
- Seven native menu rows, scoped labels/value/help hooks, duplicate/capacity guards, engine-owned option buffer, persistent mode/resolution/directory and asynchronous Windows folder picker implemented. Native Export remains unchanged with VYNE Off. In Resolution check mode, the actual Export action queues the selected mode session instead of entering the stock bake path. F6 is no longer the default workflow.
- FPS/bitrate/format/color rows are disabled and explain the unavailable backend rather than claiming nonexistent encoding. The test creates no video file. Full VYNE capture/audio/encoding remain separate open work.
- Local checks passed for native row layout, unchanged Export/action data, no inherited action/jump/warning fields, 16-row bounds, duplicate injection refusal, capability gating, Unicode Windows INI persistence, automatic ASI startup and mode-session rollback. Native menu appearance/input and native Export callback execution remain user runtime checks.

- Installed 0.3.0 after installer confirmed FiveM closed. Source/installed SHA256: 7A04E0780B4F5C7B8722AFBCE0D379801E92B990A8765327111F6FCBF2ECB559. Readback confirms EnableExportMenu=1 and EnableModeProbe=0. Prior own binary/config backed up, MinHook license included, unrelated plugins/game settings preserved. User workflow is now the native Export screen, not F6.
