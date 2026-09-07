# NeuRotic — OptiScaler DLSS-NR fork

NeuRotic is a personal experimental fork of OptiScaler focused on DLSS Neural Rendering work.

This repository is not the official OptiScaler project and is not affiliated with the upstream OptiScaler maintainers. The fork's changes are experimental and may be game-, driver-, GPU-, or API-specific.

## Alpha 0.4

[Alpha 0.4](ALPHA-0.4.md) adds:

- NR telemetry for model time, total NR time, composition/copy time, dimensions, resets, rebuilds, and evaluation failures.
- Jitter-aware Pre-SR Neural Rendering while preserving native temporal inputs and mode-dependent resolutions.
- Explicit per-mode DLSS preset routing.
- Readiness and reset hardening across resource, mode, dimension, format, and lifecycle transitions.
- Optional NR Performance Mode and clearer Neural Rendering keybind guidance.

See the [Alpha 0.4 release](https://github.com/MagicalPrincessUnicorn/NeuRotic-an-OptiScaler-DLSSNR-fork/releases/tag/alpha-0.4) for the downloadable Windows package when published.

## Installation

1. Install OptiScaler using its normal installation instructions.
2. Download the Windows package from the [Alpha 0.4 release page](https://github.com/MagicalPrincessUnicorn/NeuRotic-an-OptiScaler-DLSSNR-fork/releases/tag/alpha-0.4).
3. Back up your current OptiScaler files.
4. Extract the package into the OptiScaler/game installation directory and overwrite the matching files.
5. Start the game and open the OptiScaler overlay.

There are no Dagger or Bows prerequisites. Those names were included in an earlier documentation mistake and are not part of this fork's installation requirements.

If you download the repository source instead of the release package, it must be built first; the source tree is not a drop-in binary installation.

### Source archive versus install package

GitHub also provides automatic `Source code (zip)` and `Source code (tar.gz)` downloads. Those archives contain the development source and generic OptiScaler setup scripts; they are not the NeuRotic install package. Do not run `setup_windows.bat` from the source archive.

The setup script is generic rather than Monster Hunter Wilds-specific: it works in the folder where you extracted a compiled OptiScaler package, asks which proxy filename to use, and supports different game layouts. The Alpha release package is the intended download for normal users.

## Links

- [NeuRotic Alpha 0.4 branch](https://github.com/MagicalPrincessUnicorn/NeuRotic-an-OptiScaler-DLSSNR-fork/tree/alpha-0.4)
- [NeuRotic releases](https://github.com/MagicalPrincessUnicorn/NeuRotic-an-OptiScaler-DLSSNR-fork/releases)
- [Official OptiScaler project](https://github.com/optiscaler/OptiScaler)
- [Parent OptiScaler DLSS-NR fork](https://github.com/Dagherbou/OptiScaler_DLSSNR)
- [Parent no-rendering branch](https://github.com/Dagherbou/OptiScaler_DLSSNR/tree/dlss-neural-rendering)

## Support the fork

If NeuRotic is useful to you, you can support development on [Ko-fi](https://ko-fi.com/espiownage).

## Credits and licensing

NeuRotic builds on OptiScaler and the upstream project's dependencies. Please review the repository [LICENSE](LICENSE), the `Licenses` directory, and the upstream project for original authorship, attribution, and licensing information.
