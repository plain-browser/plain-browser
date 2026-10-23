<# Run after every `gnrt vendor` / `gnrt gen` in the ungoogled-chromium-windows tree.
   ungoogled-chromium-windows keeps the Windows import libs (windows.0.52.0.lib) out of the vendored
   crates; it downloads windows-rs and patches the generated BUILD.gn files to read them from there.
   - `gnrt vendor` deletes the extracted vendor\windows-rs-0.52.0 directory,
   - `gnrt gen` regenerates the three windows_*_msvc BUILD.gn files, losing that patch
     (and, with no lib\ files on disk, also the native_libs line).
   This script restores both. Idempotent.
   Usage: powershell -File scripts\post-gnrt-fixups.ps1 [-Src C:\dw-build\ucw\build\src] [-Ucw C:\dw-build\ucw] #>
param([string]$Src = "C:\dw-build\ucw\build\src", [string]$Ucw = "C:\dw-build\ucw")
$ErrorActionPreference = "Stop"
$vend = "$Src\third_party\rust\chromium_crates_io\vendor"
if (-not (Test-Path "$vend\windows-rs-0.52.0")) {
  $zip = "$Ucw\build\download_cache\rust-windows-create-0.52.0.zip"
  if (-not (Test-Path $zip)) { throw "windows-rs archive not in download cache: $zip" }
  Add-Type -AssemblyName System.IO.Compression.FileSystem
  [IO.Compression.ZipFile]::ExtractToDirectory($zip, $vend)
  Write-Host "extracted windows-rs-0.52.0"
}
foreach ($arch in "x86_64_msvc","i686_msvc","aarch64_msvc") {
  $f = "$Src\third_party\rust\windows_$arch\v0_52\BUILD.gn"
  $t = [IO.File]::ReadAllText($f)
  if ($t -match "windows-rs-0\.52\.0") { Write-Host "$arch already patched"; continue }
  $base = "//third_party/rust/chromium_crates_io/vendor"
  $t = $t.Replace("$base/windows_${arch}-v0_52/src/lib.rs", "$base/windows-rs-0.52.0/crates/targets/$arch/src/lib.rs")
  $t = $t.Replace("$base/windows_${arch}-v0_52/build.rs", "$base/windows-rs-0.52.0/crates/targets/$arch/build.rs")
  $t = $t.Replace('cargo_pkg_version = "0.52.6"', 'cargo_pkg_version = "0.52.0"')
  $lib = "native_libs = [ `"$base/windows-rs-0.52.0/crates/targets/$arch/lib/windows.0.52.0.lib`" ]"
  $t = [regex]::Replace($t, '(?s)(build_sources = \[.*?\]\r?\n)', "`$1    $lib`n", 1)
  [IO.File]::WriteAllText($f, $t)
  Write-Host "patched $arch (native_libs added: $($t.Contains('native_libs')))"
}
# `patch -N` leaves reject files when run over already-applied/regenerated content; remove them.
Get-ChildItem "$Src\third_party\rust","$Src\tools\rust" -Recurse -Filter *.rej -ErrorAction SilentlyContinue | ForEach-Object { Remove-Item $_.FullName -Force; Write-Host "removed $($_.Name)" }
