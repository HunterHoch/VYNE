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
