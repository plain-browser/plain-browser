# Adblock (adblock-rust) integration

Status 2026-10-23: code written, patches dry-applied against Chromium 156.0.8078.47 copies,
**not compiled yet** (the stock build was running; the crate vendoring step is still to do).

## Architecture
- `src/draftway/adblock/rs` – Rust bridge (cxx) over `adblock` 0.13.3. Default lists
  (EasyList, EasyPrivacy, EasyList Polish) are compiled in via `include_str!`.
  `cargo test` runs standalone (4 tests pass); GN ignores Cargo.toml/build.rs.
- `browser/` – `AdBlockService` (process-wide engine on its own sequence),
  `AdblockFilterImpl` (mojo receivers on that sequence), `ExposeInterfacesToRenderer`
  (registry hook, kill switch `--disable-features=DraftwayAdblock`).
- `renderer/` – `AdblockURLLoaderThrottle` (defers each http(s) subresource, asks the
  browser, cancels with `ERR_BLOCKED_BY_CLIENT`), `AdblockThrottleFactory` (owned by
  chrome's `URLLoaderThrottleProviderImpl`, cloned per worker thread like SafeBrowsing),
  `CosmeticFilterAgent` (per frame, inserts hide-selector CSS as a user stylesheet).
- `common/adblock.mojom` – `AdblockFilter { Clone, ShouldBlockRequest, GetCosmeticCss }`.

Wiring patches (`patches/30-adblock/0001..0004`) touch 6 Chromium files, 30 added lines.

## Why GN and not a cargo staticlib
Chromium links its own Rust std (`build/rust/std`) and routes Rust allocation to
PartitionAlloc (`build/rust/allocator`); cxx's C++ side is compiled with Chromium's clang +
libc++. A cargo staticlib would bring a second std/allocator and an MSVC-STL cxx.

## Remaining step: vendor the crates (patch 30-adblock/0000, generated)
18 crates of the closure are missing from `third_party/rust/chromium_crates_io`
(adblock, addr, arrayvec, flatbuffers, form_urlencoded, idna, idna_adapter,
percent-encoding, precomputed-hash, psl, psl-types, rustc-hash, rustc_version, semver,
seahash, thiserror 1, thiserror-impl 1, url). In the tree after the stock build:
1. `third_party/rust/chromium_crates_io/Cargo.toml`:
   `[dependencies.adblock]` `version = "=0.13.3"`, `default-features = false`,
   `features = ["full-regex-handling", "embedded-domain-resolver"]`.
2. `gnrt_config.toml`: `[crate.regex]` group `test` -> `safe` (else regex stays testonly);
   `allow_unsafe` / license entries for new crates as gnrt asks. Engine runs in the browser
   process for the alpha; moving parsing to a utility process is a later hardening step.
3. `python3 tools/crates/run_gnrt.py vendor` then `... gen`
   (this also enables bitflags `serde` and serde `rc` features tree-wide).
4. Export the diff (Cargo.toml/lock, gnrt_config.toml, new vendor dirs, generated
   `third_party/rust/*/v*/BUILD.gn`) as `patches/30-adblock/0000-vendor-adblock-crates.patch`.
   Redo with gnrt on every Chromium bump instead of hand-merging Cargo.lock.
Watch: flatbuffers has a build.rs (rustc_version); psl is a large generated crate.

## Not done yet (alpha scope)
- `$redirect` resources (treated as block), scriptlets, generic class/id hiding.
- Per-site toggle, toolbar counter, `chrome://adblock` page, list updates (lists are a
  build-time snapshot: refresh with the release).
- Navigation (`$document`) blocking; requests started from the browser process.
