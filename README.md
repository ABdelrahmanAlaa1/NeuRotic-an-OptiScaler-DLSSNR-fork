# NeuRotic — OptiScaler DLSS-NR fork

NeuRotic is a personal experimental fork of OptiScaler focused on DLSS Neural Rendering work.

This repository is not the official OptiScaler project and is not affiliated with the upstream OptiScaler maintainers. The fork's changes are experimental and may be game-, driver-, GPU-, or API-specific.

## Alpha 0.9.4

[Alpha 0.9.4](ALPHA-0.9.4.md) is the current public prerelease. It adds:

- Two Neural Rendering routes: **Performance (default)** runs NR before native DLSS Super Resolution, while **Quality** keeps NR after it.
- Ray Reconstruction-aware routing that preserves the game's native RR ownership and runs NR after native RR.
- GPU-completion-tracked resource retirement, synchronized NR settings, transactional allocation, safer capture resizing, and honest readiness reporting.
- A fresh Feature 18 model session after NR is re-enabled instead of resetting and reusing a retained temporal session.
- A reorganized, readable overlay with stable two-column sizing, nested text wrapping, and upper-right anchoring that expands downward.
- Correct per-mode DLSS preset/override routing and broader transition handling across mode, resolution, format, device, and lifecycle changes.

See the [Alpha 0.9.4 release](https://github.com/MagicalPrincessUnicorn/NeuRotic-an-OptiScaler-DLSSNR-fork/releases/tag/alpha-0.9.4) for the downloadable Windows package.

## Installation

1. Download `OptiScaler-DLSSNR-alpha-0.9.4.zip` from the [Alpha 0.9.4 release page](https://github.com/MagicalPrincessUnicorn/NeuRotic-an-OptiScaler-DLSSNR-fork/releases/tag/alpha-0.9.4). Do not download GitHub's automatic source archive for installation.
2. Back up the existing OptiScaler files beside the game's real executable.
3. Extract the complete package beside that executable, then run `setup_windows.bat` and choose the proxy filename appropriate for the game. `dxgi.dll` is the normal first choice for DirectX 12; Vulkan generally uses `winmm.dll`.
4. Supply your own licensed `nvngx_dlssnr.dll` model file beside the game executable. It is an NVIDIA binary and is deliberately not redistributed here. The similarly named `nvngx.dll_dlssnr.dll` forwarder is included and is required.
5. Start the game and open the OptiScaler overlay. Neural Rendering itself remains opt-in; its routing mode defaults to Performance and can be changed to Quality.

### Green/white particle “flashbang” workaround

Monster Hunter Wilds and some other games can flood the screen with bright green or white particles when NR meets the game's existing tone-mapping or post-processing path. If this happens, install [ReShade with full add-on support](https://reshade.me/) and an SDR/HDR [RenoDX mod for the affected game](https://github.com/clshortfuse/renodx/wiki/Mods). A game-specific RenoDX shader rewrite often resolves or substantially reduces the flashing.

For Monster Hunter Wilds, use [RenoDX — HDR and SDR Fix / Tonemap / Color Grade](https://www.nexusmods.com/monsterhunterwilds/mods/202) and follow that page's requirements, including REFramework and ReShade with add-on support. Its source is available on the [`mhwilds` RenoDX branch](https://github.com/MohannedElfatih/renodx/tree/mhwilds). The mod works in SDR as well as HDR; do not enable an HDR preset merely because NR is installed. When ReShade and OptiScaler share a game, follow both projects' proxy/loader instructions so they do not overwrite the same proxy DLL.

There are no Dagger or Bows prerequisites. Those names were included in an earlier documentation mistake and are not part of this fork's installation requirements.

If you download the repository source instead of the release package, it must be built first; the source tree is not a drop-in binary installation.

### Source archive versus install package

GitHub also provides automatic `Source code (zip)` and `Source code (tar.gz)` downloads. Those archives contain the development source and generic OptiScaler setup scripts; they are not the NeuRotic install package. Do not run `setup_windows.bat` from the source archive.

The setup script is generic rather than game-specific: it works in the folder where you extracted the compiled package, asks which proxy filename to use, and supports different game layouts. The Alpha release package is the intended download for normal users.

## Related tools

NeuRotic focuses on Neural Rendering; it does not itself unlock DLSS Multi Frame Generation on unsupported hardware or add native DLSS Frame Generation to a game that lacks it.

- **Recommended focused companion for RTX 40-series cards:** [Universal RTX 40 MFG Unlocker](https://github.com/dashdogy/RTX40MFG-Unlock). It targets games that already have working Streamline DLSS Frame Generation and provides fixed or Dynamic MFG controls. Read its compatibility, loader, anti-cheat, and experimental-status warnings before combining mods.
- **Alternative Ada MFG implementation:** [mfg-unlock](https://github.com/matiasLombo/mfg-unlock), a separate RTX 40-series 3x/4x project with its own pacing approach and installation constraints.
- **Broader frame-generation translation:** [DLSS Enabler](https://github.com/artur-graniszewski/DLSS-Enabler) and [DLSS Unlocked](https://github.com/ShyVortex/dlss-unlocked) provide separate all-in-one approaches for hardware or games outside NeuRotic's scope.
- **Mainline project:** [official OptiScaler](https://github.com/optiscaler/OptiScaler) remains the best source for the wider upscaler/frame-generation feature set and current upstream documentation.

These are independent projects. They are not bundled, required, or validated as a combined stack with every NeuRotic-supported game. Back up the game folder, avoid anti-cheat-protected multiplayer, and follow each project's own instructions.

## Links

- [NeuRotic Alpha 0.9.4 branch](https://github.com/MagicalPrincessUnicorn/NeuRotic-an-OptiScaler-DLSSNR-fork/tree/alpha-0.9.4)
- [NeuRotic Alpha 0.4 notes](ALPHA-0.4.md)
- [NeuRotic releases](https://github.com/MagicalPrincessUnicorn/NeuRotic-an-OptiScaler-DLSSNR-fork/releases)
- [Official OptiScaler project](https://github.com/optiscaler/OptiScaler)
- [Parent OptiScaler DLSS-NR fork](https://github.com/Dagherbou/OptiScaler_DLSSNR)
- [Parent no-rendering branch](https://github.com/Dagherbou/OptiScaler_DLSSNR/tree/dlss-neural-rendering)

## Support the fork

If NeuRotic is useful to you, you can support development on [Ko-fi](https://ko-fi.com/espiownage).

## Credits and licensing

NeuRotic builds on OptiScaler and the upstream project's dependencies. Please review the repository [LICENSE](LICENSE), the `Licenses` directory, and the upstream project for original authorship, attribution, and licensing information.
