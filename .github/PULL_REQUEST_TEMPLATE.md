## What and why

<!-- One paragraph. Link the issue if there is one. -->

## Checklist

- [ ] `python3 -I scripts/check-patches.py` passes (if `patches/` changed)
- [ ] `cargo test` in `src/plainbrowser/adblock/rs` passes (if the Rust bridge changed)
- [ ] C++ unit tests / end-to-end scripts run on a Windows build (say which, paste the result) or "not needed because ..."
- [ ] No new network request, or `docs/PRIVACY.md` and `docs/PRIVACY.pl.md` updated in this PR
- [ ] New third-party code or data listed in `NOTICE.md`
- [ ] Patch headers explain the change; one category per patch
- [ ] `CHANGELOG.md` updated under Unreleased
