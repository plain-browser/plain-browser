<# Stage 0 of the GitHub Actions build: a Chromium tree with the ungoogled-chromium and Plain Browser layers applied,
   ready for build.py's GN and ninja steps. Mirrors the setup part of ungoogled-chromium-windows' build.py (clone,
   downloads, pruning, unpacking, patches, domain substitution, Rust toolchain) step by step with the same upstream
   utilities, because build.py runs that part and ninja in one go and the Plain Browser layer has to go in between:
     prepare.py        before the patches (adds patches\series to the upstream series, drops the upstream patches in
                       patches\upstream-revert.list, copies src\plainbrowser into the tree)
     apply-branding.py after the patches
     vendor-adblock-crates.ps1 after the Rust toolchain exists and before gn gen (the GN deps patch refers to the
                       vendored crates' BUILD.gn files)
   Afterwards `build.py --ci` finds BUILD.gn and the Rust toolchain in place, skips its setup and goes on with
   args.gn, gn gen, bindgen and ninja (scripts\ci\build-stage.ps1). Same order as docs/ARCHITECTURE.md section 2.
   Usage: pwsh -File scripts\ci\prepare-tree.ps1 [-Work C:\pb] [-Profile alpha|official] #>
param(
  [string]$Work = "C:\pb",
  [ValidateSet("alpha", "official")][string]$Profile = "alpha"
)
. "$PSScriptRoot\common.ps1"
$repo = Get-RepoRoot
$tag = Get-PinnedTag
$ucw = Join-Path $Work "ucw"
$src = Join-Path $ucw "build\src"
Write-Host "Tag: $tag  Profile: $Profile  Work: $Work"
Write-DiskFree

Write-Step "version.h matches versions.toml"
Run python -I "$repo\scripts\sync-version.py" --check

Write-Step "ungoogled-chromium-windows $tag"
if (-not (Test-Path "$ucw\build.py")) {
  Run git clone --recurse-submodules --shallow-submodules --depth 1 --branch $tag https://github.com/ungoogled-software/ungoogled-chromium-windows.git $ucw
}
# Profile flags, appended to the upstream GN flags (same block as scripts\build.ps1; the marker keeps it idempotent).
# alpha: no official build, PGO, ThinLTO or PCH. official: upstream's flags unchanged (ThinLTO + PGO).
$flags = "$ucw\flags.windows.gn"
$content = (Get-Content $flags -Raw) -replace '(?s)\r?\n# PLAINBROWSER-OVERRIDE-BEGIN.*?# PLAINBROWSER-OVERRIDE-END\r?\n', ''
if ($Profile -eq "alpha") {
  $content += "`n# PLAINBROWSER-OVERRIDE-BEGIN`nis_official_build=false`nchrome_pgo_phase=0`nuse_thin_lto=false`nenable_precompiled_headers=false`ngenerate_about_credits=true`n# PLAINBROWSER-OVERRIDE-END`n"
}
Set-Content $flags -Value $content -NoNewline
Write-Host "flags.windows.gn:"; Get-Content $flags | ForEach-Object { "    $_" }

Push-Location $ucw
try {
  Write-Step "Chromium sources (ungoogled-chromium clone.py)"
  Run python "ungoogled-chromium\utils\clone.py" -o "build\src" -p win64
  Write-Probe $src "after clone"

  Write-Step "Plain Browser layer: prepare.py"
  Run python -I "$repo\scripts\prepare.py" $ucw $src

  Write-Step "Windows downloads (downloads.ini)"
  # build.py makes this directory before downloads.py runs; downloads.py itself raises FileNotFoundError without it
  New-Item -ItemType Directory -Force "build\download_cache" | Out-Null
  RunRetry 5 python "ungoogled-chromium\utils\downloads.py" retrieve -i downloads.ini -c "build\download_cache" --hide-progress-bar

  Write-Step "Prune binaries"
  # build.py only prunes the files in pruning.list; the CLI also deletes the "contingent" directories (v8/test/ among
  # them) unless told not to, and the build needs v8/test/torque/test-torque.tq.
  Run python "ungoogled-chromium\utils\prune_binaries.py" "build\src" pruning.list --keep-contingent-paths
  Write-Probe $src "after prune"

  Write-Step "Unpack downloads"
  foreach ($d in "third_party\microsoft_dxheaders\src", "third_party\devtools-frontend\src\third_party\esbuild") {
    $p = Join-Path $src $d
    if (Test-Path $p) { Remove-Item $p -Recurse -Force; New-Item -ItemType Directory $p | Out-Null }
  }
  Run python "ungoogled-chromium\utils\downloads.py" unpack -i downloads.ini -c "build\download_cache" "build\src"
  Write-Probe $src "after unpack downloads"

  Write-Step "Patches: ungoogled-chromium, then ungoogled-chromium-windows + Plain Browser"
  $patchBin = "$src\third_party\git\usr\bin\patch.exe"
  Run python "ungoogled-chromium\utils\patches.py" apply --patch-bin $patchBin "build\src" "ungoogled-chromium\patches"
  Run python "ungoogled-chromium\utils\patches.py" apply --patch-bin $patchBin "build\src" patches
  Write-Probe $src "after patches"

  Write-Step "Domain substitution"
  Run python "ungoogled-chromium\utils\domain_substitution.py" apply -r "ungoogled-chromium\domain_regex.list" -f domain_substitution.list "build\src"
  Write-Probe $src "after domain substitution"

  Write-Step "Branding (product name in UI strings and BRANDING)"
  Run python -I "$repo\scripts\apply-branding.py" $src

  # build.py copies the unpacked Rust toolchains into third_party\rust-toolchain (bin from the x64 toolchain, lib from
  # all three) and writes INSTALLED_VERSION; it skips the step when that file exists. gnrt (next step) needs cargo
  # from there before build.py runs, so do the same copy here.
  Write-Step "Rust toolchain"
  $rt = "$src\third_party\rust-toolchain"
  New-Item -ItemType Directory -Force "$rt\bin", "$rt\lib" | Out-Null
  foreach ($tc in "rust-toolchain-x64", "rust-toolchain-x86", "rust-toolchain-arm") {
    foreach ($comp in Get-ChildItem "$src\third_party\$tc" -Directory -ErrorAction SilentlyContinue) {
      if ($tc -eq "rust-toolchain-x64" -and (Test-Path "$($comp.FullName)\bin")) { RunRobocopy "$($comp.FullName)\bin" "$rt\bin" /E }
      if (Test-Path "$($comp.FullName)\lib") { RunRobocopy "$($comp.FullName)\lib" "$rt\lib" /E }
    }
  }
  & "$src\third_party\rust-toolchain-x64\rustc\bin\rustc.exe" --version | Set-Content "$rt\INSTALLED_VERSION"
  Write-Host "rustc: $(Get-Content "$rt\INSTALLED_VERSION")"

  Write-Step "Vendor the adblock crates (gnrt vendor + gen)"
  Run powershell -NoProfile -ExecutionPolicy Bypass -File "$repo\scripts\vendor-adblock-crates.ps1" -Src $src -Ucw $ucw
} finally {
  Pop-Location
}

Write-Step "Tree ready"
Write-Probe $src "tree ready" -Require
Write-Host "Plain Browser sources: $(@(Get-ChildItem "$src\plainbrowser" -Recurse -File).Count) files"
Write-Host "Series tail:"; Get-Content "$ucw\patches\series" | Select-Object -Last 3 | ForEach-Object { "    $_" }
Write-DiskFree
