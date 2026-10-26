# Native Export menu — VYNE 0.4.0

Open `VYNE_Test` and navigate to Rockstar Editor's existing Export screen. VYNE inserts seven rows above the original Export action, using the game's own list, focus, labels, values, disabled-state handling and linked help panel.

| Row | Current behavior |
| --- | --- |
| VYNE export | Cycles Native export, Render export and Resolution check. Render export uses the selected dimensions with native audio/timing. |
| VYNE resolution | Enabled in Render export or Resolution check; cycles 1920×1080, 2560×1440, 3840×2160. Native export shows Game settings. |
| VYNE frame rate | Disabled until VYNE replay timing is implemented. Stock frame rate remains usable with Native export. |
| VYNE bitrate | Disabled until VYNE encoding is implemented. Stock bitrate remains usable with Native export. |
| VYNE file format | Disabled; no unsupported MP4/PNG option is presented as working. |
| VYNE color profile | Disabled until real color processing/tagging exists. |
| Output location | Shows only the folder name, abbreviated to 12 UTF-16 units without splitting a surrogate pair. Full path appears in linked help. Left/right opens the Windows folder picker; Unicode paths persist. Default: Windows Videos/VYNE. |

For selected 4K output, choose **Render export**, set **VYNE resolution → 3840 x 2160**, and press the existing **Export** action. Stock Frame Rate and Bit Rate also configure Render export. See [resolution export](resolution-export.md) for behavior, evidence and runtime limits.

When the folder picker closes, it publishes a redraw request. The native selectability callback consumes it during focused input processing on the same thread that builds the editor menu, outside the recursive build/greying pass. This updates the cached Scaleform value without the worker calling engine UI code. Cancel also requests a redraw to clear the choosing state. Runtime confirmation of the new redraw remains pending.

For a viewable clip, select **Native export**, choose **Output location** with left/right, and press the original **Export** action. VYNE watches the local Rockstar `videos/rendered` directory for a new or changed MP4. It waits for stable metadata, a closed writer and complete MP4 container boxes before copying. Existing destination clips receive numbered filenames; the gallery original is preserved. A `VYNE-last-export.txt` receipt records source and destination. Cancelled exports time out after 30 minutes; a new export replaces the pending watcher. Native output keeps Rockstar's actual resolution and frame rate; selecting VYNE resolution does not upscale native exports.

For the resolution check, set **VYNE export → Resolution check**, choose **3840×2160**, then press the **existing Export action**. VYNE intercepts the native export trigger, queues the selected game mode, checks backbuffer dimensions across three distinct frames, holds for five seconds, and restores the captured mode. F7/focus loss cancels. This deliberately creates no media file; it is the resolution feasibility gate, not a completed export pipeline. The F6 hotkey is disabled by the default installer.

Settings: `plugins/VYNE/Export.ini`. Logs: `plugins/VYNE/export-menu.log` and `plugins/VYNE/mode-probe.log`. Do not drive the editor automatically; the user performs runtime checks.

Rows beyond the native 16-row limit are refused. Replacement option storage uses the verified game allocator/free pair, and the native menu owns/frees the new buffer on close. Original actions are copied intact; inserted rows inherit no action, warning or jump. Each editor rebuild re-reads the current parsed array. Text replacement is scoped to the menu currently being built, including recursive help-panel builds.

Independent build3258 mapping from the user's loaded executable: native array RVA `0x201BF98`; 56-byte menu entries and 40-byte options; BuildMenu `0x15EFF8`, AdjustToggle `0x15CC74`, toggle value `0x1811E0`, text wrapper `0xDC05FC`, selectability `0x192B1C`, Export trigger `0x1B6A34`, allocator/free `0x12E0`/`0x1318`. Code fingerprints are checked before detours. Unsupported or conflicting entries leave the native screen untouched. Public REPlus was read as an architectural reference; no source or signature table was imported into production.

Build checks cover unchanged native Export options, action isolation, capacity limits, duplicate insertion, capability gating, Unicode settings persistence, bounded hook retries, loading and mode-session rollback. Output checks reject unfinished/open-writer files and verify Unicode delivery, collision handling and byte preservation, including the user's existing native test MP4. They do not prove native menu appearance, controller input, folder-dialog behavior, game-triggered delivery or resolution switching. Those remain user runtime checks.

The previous build loaded but logged hook activation failure and rolled back. Version 0.3.1 retries MinHook's transient snapshot/allocation failure status within two seconds and logs the exact terminal status. It also corrects option Context/Block/Action field names and offsets after comparing public REPlus row definitions. That comparison did not establish the exact old activation failure, which lacked an error code.
