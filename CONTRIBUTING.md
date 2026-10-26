# Contributing to Plain Browser

Thanks for your interest. Plain Browser is a thin layer (patches plus `src/plainbrowser`) on top of
ungoogled-chromium for Windows 11. Read [README.md](README.md) and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
first; the design rules below follow from them.

## Ground rules

- **As little as possible in Chromium files, as much as possible in `src/plainbrowser`.** A patch costs work on
  every Chromium release; a file under `//plainbrowser` keeps compiling until an API it uses changes.
- **One patch, one category.** `patches/series` lists the order; directory names (`30-adblock`, `80-fingerprint`, ...)
  are the categories. Every patch starts with a header that says what it does and why, so a rebase can decide what
  to drop. No Google hostnames in hunks (see the comment at the top of `patches/series`).
- **No new network calls without a documented reason.** The browser talks only to the servers listed in
  [docs/PRIVACY.md](docs/PRIVACY.md). A change that adds a request needs a privacy-policy update in the same PR.
- **Licenses.** Our code is MPL-2.0, patches are BSD-3-Clause. Code ported from brave-core stays MPL-2.0 with its
  header. New third-party data (filter lists, resources) needs an entry in [NOTICE.md](NOTICE.md).

## Before you open a pull request

1. Patches must apply to the pinned tag in `versions.toml` without a Chromium checkout:
   ```bash
   python3 -I scripts/check-patches.py
   ```
   CI runs the same check (`patches-check.yml`).
2. Rust: `cargo test` in `src/plainbrowser/adblock/rs` (`rust.yml`).
3. Fingerprint script: `node src/plainbrowser/fingerprint/renderer/fingerprint_script_test.js` (`fingerprint-script.yml`).
4. C++ unit tests and end-to-end tests need the Windows build tree; see [docs/TESTING.md](docs/TESTING.md) and the
   `scripts/test-*.ps1` scripts. Say in the PR which of them you ran and paste the result.
5. Commit messages: imperative, first line under 72 characters, prefixed with the area
   (`Safe Browsing: ...`, `adblock: ...`, `docs: ...`). Sign your commits (`main` requires signed commits).

## Reporting bugs and ideas

Use the issue templates. For security problems do **not** open an issue; follow [SECURITY.md](SECURITY.md).

## Building

Windows only, several hours, see `scripts/build.ps1` and [docs/CI.md](docs/CI.md). You do not need a build to
contribute to patches, lists, docs, the website (`website/`) or the Safe Browsing proxy (`infra/`).
