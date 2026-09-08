# NeuRotic Alpha 0.9.4

Alpha 0.9.4 is a public prerelease focused on making DLSS Neural Rendering substantially safer, clearer, and more practical during normal gameplay and rendering transitions. It consolidates the accepted Alpha 0.9.4 UI, lifecycle, GPU-safety, toggle-safety, and robustness work.

This remains experimental rendering middleware. Results can vary with the game, GPU, driver, rendering API, DLSS files, and other injected tools.

## Highlights

- **Performance Mode is now the default NR route.** It runs Neural Rendering before the game's native DLSS Super Resolution so the model processes fewer pixels. The overlay can switch to **Quality**, which keeps NR after native Super Resolution for the established full-resolution path.
- **Ray Reconstruction-aware operation.** When a game is using native DLSS Ray Reconstruction, the game retains native RR reconstruction/upscaling ownership and NeuRotic runs NR afterward. This avoids forcing the Performance route across an incompatible native RR boundary and improves compatibility with ray-traced titles that use RR.
- **Much stronger long-session GPU lifetime safety.** NR command-list recordings are paired with actual queue submissions and fence completion. Descriptor/upload rings, timer/readback slots, capture surfaces, scalers, private DLAA resources, proxy resources, and retired model sessions are no longer reclaimed or overwritten merely because a fixed number of frames passed.
- **Safer NR enable/disable transitions.** NR configuration publication is synchronized, rendering consumes one immutable settings snapshot per host upscale, and re-enabling NR retires the old Feature 18 temporal session before creating a fresh one.
- **Transactional allocation and cleanup.** Size-dependent scratch resources, meter/scanner rings, guide clones, Vulkan images, and readbacks are published only as complete bundles. Partial failure preserves the last valid live state and releases unpublished pieces.
- **More accurate readiness and diagnostics.** The UI distinguishes disabled, loaded, waiting/reset, quarantined, failed, and successfully evaluating states for the current route and enable generation. Stale model timing is hidden while a requested route is not actually ready.
- **Capture and exposure-scan hardening.** Capture requests survive temporary unsupported shapes, resize safely between batches, reject invalid inputs, and only read completed submissions. Exposure scanner selection, retained readbacks, output bounds, and status text are synchronized.
- **Correct per-mode DLSS preset routing.** Explicit per-mode choices remain distinct from global and NVIDIA-default behavior, and upscaler override state is applied consistently instead of silently replacing an explicit selection.

## Menu and user-interface improvements

- Reorganized the overlay so Neural Rendering and common controls are easier to find.
- Kept the performance graphs and primary menu actions visible at the top.
- Fixed the first-open horizontal growth bug without sacrificing readable column widths.
- Wrapped long text inside both main columns and compensated for nested-section indentation.
- Anchored the main window to the upper-right corner on first appearance and when its saved scale changes.
- Expanded sections now grow downward instead of repeatedly recentering the window vertically.
- Added clearer Performance/Quality route descriptions, active-state messages, keybind guidance, and distinct control IDs.

## Neural Rendering lifecycle and device hardening

- Closes NR generations before native NGX shutdown on D3D12 and Vulkan paths.
- Serializes host NR evaluation against teardown and blocks new evaluations once session closure starts.
- Cleans up caller-owned capability parameter blocks and resets proxy discovery/retry state between valid sessions.
- Retires private DLAA features, model outputs, scanner resources, and related cached state with the owning D3D12 session.
- Detects unexpected device identity changes and fails closed instead of reusing device-bound resources.
- Handles repeated/idempotent shutdown paths without double release.
- Preserves callback code lifetime for late command-list/queue notifications.
- Uses bounded capacity and conservative NR bypass under extreme backlog instead of guessing that GPU work is finished.

## Robustness and transition fixes

- Resource-state cleanup now covers early-return and failure paths during encode, model evaluation, downsample, composition, and resolve.
- Successful readiness advances only after the complete NR result reaches its destination.
- Placement changes cannot schedule both Pre-SR and Post-SR NR around one native upscale call.
- Capture resize, format, mip, and canceled-batch transitions retire pending work safely.
- Resolution and DLAA transitions use the current logical and subrect dimensions rather than stale native-RR dimensions.
- The disabled-by-default driver-proxy experiment now follows the same resolve/cleanup contract; it remains opt-in and has no automatic fallback.
- Vulkan allocation helpers unwind partially created image, memory, view, readback, sampler, layout, pool, and descriptor resources.

## Configuration included in this package

The packaged `OptiScaler.ini` is the tested profile saved from the accepted Crimson Desert validation session, as requested for this release. Important points:

- Neural Rendering itself remains opt-in.
- `RenderingMode=1` selects **Performance (Default)**; choose **Quality** in the overlay if preferred.
- The saved profile does not force the DLSS render-preset override globally.
- File logging is enabled to make Alpha problem reports useful.

Back up an existing `OptiScaler.ini` before extraction if you want to retain game-specific settings. This public profile is a starting point, not a claim that one configuration is ideal for every title.

## Installation

1. Download **`OptiScaler-DLSSNR-alpha-0.9.4.zip`** from the GitHub release. GitHub's automatic **Source code** archives are not install packages.
2. Fully close the game and back up its existing OptiScaler files and INI.
3. Extract every file from the release ZIP beside the game's real executable.
4. Run `setup_windows.bat`. For DirectX 12, `dxgi.dll` is the normal first proxy choice; Vulkan generally uses `winmm.dll`. A particular game may require another supported proxy.
5. Supply your own licensed NVIDIA **`nvngx_dlssnr.dll`** model beside the game executable. This proprietary model is not included. Do not confuse it with the included **`nvngx.dll_dlssnr.dll`** forwarder; both filenames serve different purposes.
6. Start the game, open the OptiScaler overlay, and enable Neural Rendering. Confirm that the status reports successful evaluations and that the scene visibly updates before judging performance.

## Known green/white particle “flashbang” issue

Monster Hunter Wilds and some other games can fill the screen with bright green or white particles when Neural Rendering interacts with the game's tone-mapping or post-processing shaders. If this happens, install [ReShade with full add-on support](https://reshade.me/) and an SDR/HDR [RenoDX mod made for that game](https://github.com/clshortfuse/renodx/wiki/Mods). The game-specific shader rewrite often resolves or substantially reduces the flashing; it is a workaround, not a guarantee for every title or renderer.

For **Monster Hunter Wilds**, the tested companion is [RenoDX — HDR and SDR Fix / Tonemap / Color Grade](https://www.nexusmods.com/monsterhunterwilds/mods/202). Follow that page's current installation requirements: install REFramework, install ReShade with add-on support, then extract the RenoDX package beside `MonsterHunterWilds.exe`. No ReShade effects are required. The corresponding source is available on the [`mhwilds` branch of MohannedElfatih/renodx](https://github.com/MohannedElfatih/renodx/tree/mhwilds).

Use the RenoDX SDR path for SDR output or its HDR path for HDR output. Avoid double tone mapping: if the picture becomes washed out, check the RenoDX guidance and disable competing Auto HDR or RTX HDR processing. ReShade and OptiScaler may both want a proxy DLL name, so follow their coexistence/loader instructions and never overwrite an unrelated loader blindly.

## Related tools for features outside NeuRotic's scope

NeuRotic Alpha 0.9.4 does not itself unlock DLSS Multi Frame Generation on unsupported GPUs and does not add native DLSS Frame Generation to games that do not already expose it.

- **Recommended for a focused RTX 40-series MFG companion:** [Universal RTX 40 MFG Unlocker](https://github.com/dashdogy/RTX40MFG-Unlock). It is intended for supported games that already provide working Streamline DLSS Frame Generation and includes fixed/Dynamic controls.
- [mfg-unlock](https://github.com/matiasLombo/mfg-unlock) is an alternative RTX 40-series 3x/4x implementation with a different loader and pacing design.
- [DLSS Enabler](https://github.com/artur-graniszewski/DLSS-Enabler) and [DLSS Unlocked](https://github.com/ShyVortex/dlss-unlocked) offer broader, separate frame-generation and compatibility approaches.
- [Official OptiScaler](https://github.com/optiscaler/OptiScaler) is the mainline project for the broader upscaler and frame-generation feature set.

These are independent projects, not bundled dependencies. Combined compatibility is game-specific and has not been certified here. Read each project's requirements and warnings, avoid anti-cheat-protected multiplayer, and keep a reversible backup.

## Validation and known limits

Alpha 0.9.4 passed the project's non-game D3D12 WARP lifetime/fence tests, transactional allocation and failure-injection tests, capture-resize transitions, settings concurrency tests, exposure-scan concurrency/bounds tests, readiness transitions, and 10,000 synthetic toggle cycles. The native Vulkan path was compiled and reviewed but did not receive equivalent real-game Vulkan fault testing.

User testing confirmed normal cold start, rendering, route changes, window-mode changes, and paced NR enable/disable use well enough to designate this build viable as an Alpha. It is not a universal compatibility certification or a guarantee against driver faults.

**Known stress limit:** deliberately spamming NR on/off in rapid bursts eventually produced an NVIDIA `nvlddmkm` Event 153 after 26 full Feature 18 retire/recreate cycles in about 20 seconds. Ordinary paced transitions worked in the accepted test. Do not hammer the toggle key; allow the current transition and resumed rendering to settle before toggling again.

Other limits:

- The NVIDIA Feature 18 model is proprietary and is not included.
- Native Vulkan allocation/lifecycle changes have less runtime coverage than D3D12.
- The optional exposure scanner still relies on the host game's foreign-resource state and heap-lifetime contract.
- Unsupported command-list/queue hook combinations may conservatively skip or retain NR work rather than risk premature reuse.
- Third-party injectors, overlays, frame-generation tools, and anti-cheat systems can introduce independent compatibility issues.

## Performance consequences

Performance mode reduces the model's pixel workload by placing NR before native Super Resolution. Exact savings depend on render resolution and game behavior. Quality mode performs NR after Super Resolution and normally costs more.

The safety work adds lightweight CPU bookkeeping and a private fence signal after relevant D3D12 submissions. It does not add an ordinary per-frame CPU wait. Under backlog, replay, transition, or uncertain ownership, NeuRotic may bypass an NR frame or retain resources longer rather than reuse them unsafely. Re-enabling NR may skip one or more frames and pay model-creation latency; there is no added steady-state recreation cost.

## Release identity

- Public branch/tag: `alpha-0.9.4`
- Implementation branch: `exp/alpha-0.9.4-robustness`
- Implementation commit: `c50e1c10bda8870ba55a9af92208f50f5b2e86db`
- Status: public experimental prerelease; accepted Alpha candidate, not stable-branch promotion
- Windows build: Release/x64, packaged as a matched OptiScaler/NR-forwarder pair

## Credits

NeuRotic builds on [OptiScaler](https://github.com/optiscaler/OptiScaler) and the [OptiScaler DLSS-NR fork](https://github.com/Dagherbou/OptiScaler_DLSSNR). The NR colour-composition work includes design derived from [RenoDX](https://github.com/clshortfuse/renodx); its attribution and license notice are included in the package. Review `LICENSE` and the `Licenses` directory for the full notices.
