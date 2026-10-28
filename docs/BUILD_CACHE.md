# Faster rebuilds: Siso, remote execution, compile caches

Status 2026-10-28: investigation only, nothing changed in the build. Figures marked **measured** come from the build
VM's tree (`C:\dw-build\ucw\build\src`, alpha profile, 12 vCPU): its `out\Default\.ninja_log`, `ninja -t deps` and
`ninja -t commands`, and from the GitHub mirror's compare of `156.0.8078.47...156.0.8078.73`. Figures marked
**estimate** are not measured.

## Where the time goes

| | |
|---|---|
| Full build of the alpha profile | **measured** 7.0 h wall in three ninja runs (3.50 + 2.38 + 1.12 h), 59,500 build steps, 69 CPU hours, ninja keeps about 10 of 12 cores busy |
| C/C++ compiles | **measured** 48,700 objects, 97 % of the CPU time (Blink and V8: 35 %) |
| Everything else (mojom, grit, other code generation, Rust, links) | **measured** under 3 % together; Rust is 0.2 %, links (no LTO in the alpha profile) are negligible |
| A rebuild after a change to our layer | **measured** 12-35 min wall: every one of the four this week recompiled about 835 objects in `chrome/browser` (2 CPU hours) |

A faster build therefore means fewer C++ compiles (a cache) or more cores (remote execution). Nothing else matters.

### Our own rebuilds: the strings
`chrome/grit/generated_resources.h` is included by **823 objects** (**measured**, deps log), and it was regenerated in
all four layer rebuilds this week, which recompiled 835-838 objects in `chrome/browser` each time. Our UI strings are
a `<part>` of `chrome/app/generated_resources.grd` (`patches/60-ui/0008`), so adding or renaming one of our string
IDs rewrites that header. Moving them into a grd of our own (`//plainbrowser/shields/resources`, with its own
`plainbrowser_strings.h`, loaded as a separate pak) should bring a string change down to the few dozen files that
use the strings (**estimate**). This is independent of any cache and the cheapest change in this document.

## What a version bump costs
A new upstream tag means a new source tarball, so `scripts/build.ps1` and CI build a fresh tree: every file has a new
modification time and ninja recompiles all 48,700 objects, even though `156.0.8078.47 -> .73` changed only 294 files
(299 commits; **measured**). Of those, 39 headers and 109 sources matter for C++:

| Objects that must be recompiled for .47 -> .73 | Objects | Compile CPU |
|---|---|---|
| including one of the 39 changed headers | **measured** 8,486 | **measured** 21.3 h (31 %) |
| ... or a generated header that may change too (version, `components_strings.h`, `generated_resources.h`) | **measured** 10,169 | **measured** 25.4 h (37 %) |

The expensive part of this bump is `third_party/blink/renderer/platform/wtf/hash_table.h` (8,315 objects). Other patch
releases will differ. A cache keyed on content would turn the 7 h bump into roughly **2.5-3 h** on the same VM
(**estimate**: 31-37 % of the compile time, plus a cache lookup for the rest; with sccache on clang-cl each hit still
runs the preprocessor).

## Siso and remote execution (RBE)
- **The tree has no Siso.** `third_party/siso` holds only `LICENSE`, `OWNERS` and `README.chromium` (the binary comes
  from CIPD, which ungoogled does not fetch); `build/config/siso/*.star` is there. ungoogled-chromium-windows'
  `build.py` runs its own pinned `third_party\ninja\ninja.exe` 1.12.1 (`downloads.ini`), `args.gn` sets neither
  `use_siso` nor `use_remoteexec`, and none of our scripts mention Siso (**measured**). Siso could be built from source
  with the Go 1.25 toolchain the tree already downloads for Dawn (**estimate**, not tried).
- **The Windows toolchain is outside the tree.** Every compile command uses the in-tree
  `third_party\llvm-build\Release+Asserts\bin\clang-cl.exe`, but includes the system headers as
  `-imsvc../../../../../../Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/include`
  and `.../Windows Kits/10/include/10.0.28000.0/{ucrt,um,shared,winrt}` (**measured**). These paths leave the source
  tree, so a remote worker cannot receive them as inputs. Chromium's own Siso config only describes the Windows SDK for
  a remote action when `build/win_toolchain.json` exists, which is absent with `DEPOT_TOOLS_WIN_TOOLCHAIN=0`
  (`build/config/siso/win_sdk.star`). The same applies to a remote *cache* through Siso or reclient: the action digest
  is built from the inputs under the exec root.
- **What it would take**: package the VS 2022 Build Tools and the 10.0.28000 SDK into the tree the way depot_tools does
  (`vs_toolchain.py` package format and a `win_toolchain.json`), switch to `DEPOT_TOOLS_WIN_TOOLCHAIN=1` with a local
  hash, write a `rewrapper_windows.cfg`-style platform for our own workers, and run Windows (or Linux with a
  case-insensitive SDK copy, as Google does) workers next to a BuildBuddy or NativeLink scheduler. That diverges from
  how ungoogled-chromium-windows builds, which we follow on every bump. **Not worth it for one builder** (**estimate**):
  the Unraid host would run the workers on the same hardware the VM already uses, so remote execution adds cores only
  if the host has idle ones, while the cache part is available far more cheaply (below).

## Smaller steps, cheapest first

1. **Our strings in their own grd** (above): only rebuilds of our layer, about 15 min saved per string change.
2. **sccache as `cc_wrapper`, local disk cache.** Chromium supports `cc_wrapper="sccache"` with clang-cl on Windows
   (`build/toolchain/win/toolchain.gni` prefixes the compiler and turns off the `/showIncludes:user` optimisation that
   sccache cannot handle). One GN arg in `scripts/build.ps1`'s override block, one binary on the VM, a cache directory
   on the build drive. Expected: a bump in about 2.5-3 h instead of 7 h, and a re-run of the same tree (a fresh CI
   tree, a rebuilt VM) close to cache-only time (**estimate**). The command lines contain absolute paths only through
   the relative `-imsvc` paths above, which stay the same as long as the tree stays at `C:\dw-build\ucw` and the VS and
   SDK versions do not change; a VS or SDK update empties the cache in practice. Rust is too small to bother with.
3. **The same cache for CI**: sccache's S3 backend against a Cloudflare R2 bucket (no egress fees; we already run
   Cloudflare Workers), so the staged `build-windows.yml` jobs share objects across runs. Needs a bucket and two
   secrets; the 10 GB GitHub Actions cache is too small for this tree (`out\Default` is 11 GB, **measured**).
4. **Keeping `out\Default` across bumps** by copying it into the new tree and giving every unchanged source the old
   modification time. No new tool, but correctness depends on modification times and on generated files, and it does
   not help CI. Only if sccache turns out not to work.
5. **Siso + BuildBuddy/NativeLink**: only with a second build machine, after the toolchain packaging above.

## Next steps
1. Move the shields and adblock strings out of `generated_resources.grd` into a grd under `//plainbrowser`.
2. On the VM: install sccache (pinned release, checksum in `scripts/install-tools.ps1`), add
   `cc_wrapper="sccache"` to the alpha override, build the current tag once to fill the cache, then measure a
   `.47 -> .73` bump: wall time, hit rate (`sccache --show-stats`), cache size.
3. If step 2 delivers, the R2 backend for CI (secrets `SCCACHE_BUCKET`, `SCCACHE_ENDPOINT`, keys), documented in
   `docs/CI.md`.
4. Revisit remote execution only if a second machine becomes available.
