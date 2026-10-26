# Native Export menu — VYNE 0.3.0

Open `VYNE_Test` and navigate to Rockstar Editor's existing Export screen. VYNE inserts seven rows above the original Export action, using the game's own list, focus, labels, values, disabled-state handling and linked help panel.

| Row | Current behavior |
| --- | --- |
| VYNE export | Left/right selects Off or Resolution check. Off retains the stock exporter. |
| VYNE resolution | Cycles 1920×1080, 2560×1440, 3840×2160; saved between sessions. |
| VYNE frame rate | Disabled until VYNE replay timing is implemented. Stock frame rate remains usable with VYNE Off. |
| VYNE bitrate | Disabled until VYNE encoding is implemented. Stock bitrate remains usable with VYNE Off. |
| VYNE file format | Disabled; no unsupported MP4/PNG option is presented as working. |
| VYNE color profile | Disabled until real color processing/tagging exists. |
| VYNE project directory | Left/right opens the Windows folder picker on a separate thread; Unicode paths persist. |

For the resolution check, set **VYNE export → Resolution check**, choose **3840×2160**, then press the **existing Export action**. VYNE intercepts the native export trigger, queues the selected game mode, checks backbuffer dimensions across three distinct frames, holds for five seconds, and restores the captured mode. F7/focus loss cancels. This deliberately creates no media file; it is the resolution feasibility gate, not a completed export pipeline. The F6 hotkey is disabled by the default installer.

Settings: `plugins/VYNE/Export.ini`. Logs: `plugins/VYNE/export-menu.log` and `plugins/VYNE/mode-probe.log`. Do not drive the editor automatically; the user performs runtime checks.

Rows beyond the native 16-row limit are refused. Replacement option storage uses the verified game allocator/free pair, and the native menu owns/frees the new buffer on close. Original actions are copied intact; inserted rows inherit no action, warning or jump. Each editor rebuild re-reads the current parsed array. Text replacement is scoped to the menu currently being built, including recursive help-panel builds.

Independent build3258 mapping from the user's loaded executable: native array RVA `0x201BF98`; 56-byte menu entries and 40-byte options; BuildMenu `0x15EFF8`, AdjustToggle `0x15CC74`, toggle value `0x1811E0`, text wrapper `0xDC05FC`, selectability `0x192B1C`, Export trigger `0x1B6A34`, allocator/free `0x12E0`/`0x1318`. Code fingerprints are checked before detours. Unsupported or conflicting entries leave the native screen untouched. Public REPlus was read as an architectural reference; no source or signature table was imported into production.

Build checks cover unchanged native Export options, action isolation, capacity limits, duplicate insertion, capability gating, Unicode settings persistence, loading and mode-session rollback. They do not prove native menu appearance, controller input, folder-dialog behavior or resolution switching. Those remain user runtime checks.
