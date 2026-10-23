# Draftway (alpha scaffold)

Privacy browser for Windows 11: ungoogled-chromium base, built-in adblock-rust, curated MV2 extensions.
Design: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Status (0.1.0-alpha.1)
| Part | State |
|---|---|
| adblock-rust FFI crate | Done, 3 unit tests pass (`cargo test`) |
| C++ AdBlockService + GN wiring | Written, **not compiled** (needs Chromium tree) |
| Build orchestration (build.ps1, prepare.py) | Written, **not run** (needs Windows builder) |
| Branding, updater, throttle hook, MV2 patches | **Not started** – must be generated against a real checkout |
| Installable binary | **None yet** |

## Next
1. Set the tag in `versions.toml`, run `scripts/build.ps1` on a Windows builder to get stock ungoogled-chromium building.
2. Generate patches against that tree into `patches/` and list them in `patches/series`.
