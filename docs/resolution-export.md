# Selected-resolution export — 0.4.0

Open VYNE_Test in the existing Rockstar Editor Export menu. Set **VYNE export → Render export**, **VYNE resolution → 3840 x 2160**, and select the output folder. Use the stock Frame Rate and Bit Rate rows, then press the original **Export** action. Render export is the default when migrating an older configuration without a ResolutionExport key. Native export and the no-file Resolution check remain separate modes.

This independently authored integration uses GTA's existing bake pipeline. The engine handles replay progression, markers, timestamps, audio, compression and its native presentation/watermark behavior. An independent clean capture/encoding pipeline, additional formats, custom frame rates and color controls remain unfinished. Do not infer those features from this resolution implementation.

## Why output stayed 1080p

Independent inspection of local build 3258 found a dimension helper at RVA 0x31E3DC that clamps width to 1920 and height to 1080 before aspect fitting. Its four callers feed the native capture texture and encoder configuration. Changing display mode alone leaves this clamp in place.

VYNE detours that helper with exact body/clamp guards. Only the four mapped callers receive selected dimensions, only inside a VYNE export after mode stabilization. Other calls, gameplay and Native export retain original behavior. Shared constants and persistent game settings are not patched.

## Session and evidence

1. Snapshot the existing windowed native mode, write recovery information, request selected dimensions through the game-owned mode path and wait for three distinct frames with matching swapchain buffers.
2. Dispatch native TriggerExport on the editor UI thread. The capture texture and encoder configuration receive selected dimensions.
3. Observe the GPU texture created inside the verified VideoEncode constructor at RVA 0x325594. Its first draw must have a matching viewport and a full-resolution color texture bound to the pixel shader. Missing or mismatched evidence prevents successful delivery. This does not independently prove absence of later native overlays/watermarks.
4. A forwarding IMFSinkWriter wrapper checks input/output video sizes, observes errors/cancellation and waits for successful Finalize. Samples, audio and timestamps pass unchanged.
5. After native playback closes, release held capture references, restore the original mode and verify original queue/buffer dimensions. F7 requests native delayed close first; VYNE does not tear playback down inside Present. Encoder finalization wait is bounded to 30 seconds and active bake to 30 minutes. Hard crashes can defeat immediate restoration; recovery information remains for review.
6. Deliver only after GPU verification, encoder success and restoration. The MP4 must be closed, stable, complete and have the selected track dimensions. Wrong dimensions and failed/cancelled sessions are not delivered as successful exports. Existing destination files receive numbered names.

Logs beside the ASI in VYNE/: resolution-export.log records hook/GPU/encoder evidence; mode-probe.log records selected/original mode and restoration; export-menu.log records UI dispatch.

The adapter currently accepts an existing windowed swapchain on FiveM build 3258. Fullscreen, DSR/VSR and alternate internal scaling remain unverified. Actual VYNE_Test output and restoration require user runtime validation.

## Local checks

MSVC warnings-as-errors build; isolated ASI loader; transactional graphics-mode tests; native row/settings/mode checks; finished-file delivery and wrong-size rejection. The platform test tests/mf_4k_test.cpp encodes four synthetic NV12 frames with Windows H.264. ffprobe reports Constrained Baseline, 3840x2160, level 5.1 and four frames. This verifies local codec support, not GTA rendering or audio sync.

References: [Microsoft H.264 encoder](https://learn.microsoft.com/en-us/windows/win32/medfound/h-264-video-encoder), [Direct3D texture descriptors](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createtexture2d). Public REPlus/EVER supplied architectural leads; no upstream implementation or pattern table was imported into VYNE.
