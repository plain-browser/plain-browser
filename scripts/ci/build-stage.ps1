<# One build stage of the GitHub Actions build: a single `build.py --ci` pass on the prepared tree.
   With --ci build.py skips every finished setup step, writes args.gn and runs gn gen and bindgen on the first pass,
   and then runs ninja for at most 3.5 hours. It exits 124 when that time slice ends (the tree is then handed to
   the next job through an artifact) and 0 when chrome, chromedriver and mini_installer are built (it then also runs
   upstream's package.py; our packaging follows in scripts\ci\package.ps1). Same mechanism and exit code as
   ungoogled-chromium-windows' own .github/actions/stage.
   Writes finished=true|false to GITHUB_OUTPUT.
   Usage: pwsh -File scripts\ci\build-stage.ps1 [-Work C:\pb] [-Profile release|alpha|official] [-Jobs N] #>
param(
  [string]$Work = "C:\pb",
  [ValidateSet("release", "alpha", "official")][string]$Profile = "release",
  [int]$Jobs = 0
)
. "$PSScriptRoot\common.ps1"
$ucw = Join-Path $Work "ucw"
if (-not (Test-Path "$ucw\build\src\BUILD.gn")) { throw "no prepared tree in $ucw (run prepare-tree.ps1 or unpack-tree.ps1 first)" }
if ($Jobs -le 0) {
  # Standard runners have 4 cores and 16 GB of RAM. The release and alpha profiles link without LTO and fit 4 parallel jobs;
  # official (ThinLTO + PGO) needs the headroom upstream leaves with -j 2.
  $Jobs = if ($Profile -eq "official") { 2 } else { 4 }
}
Write-DiskFree

# Compile cache (docs/BUILD_CACHE.md): with SCCACHE_BUCKET set (workflow variable) and sccache installed by
# install-tools.ps1, every compiler call goes through sccache, whose S3 backend is a Cloudflare R2 bucket shared by all
# stages and all runs. build.py rewrites args.gn from flags.windows.gn on every pass, so the argument is added there;
# without a bucket the block is removed again, so a tree handed over from a cached run builds normally.
$flags = "$ucw\flags.windows.gn"
$flagText = (Get-Content $flags -Raw) -replace '(?s)\r?\n# PLAINBROWSER-SCCACHE-BEGIN.*?# PLAINBROWSER-SCCACHE-END\r?\n', ''
$useCache = [bool]$env:SCCACHE_BUCKET
if ($useCache) {
  if (-not (Get-Command sccache -ErrorAction SilentlyContinue)) { throw "SCCACHE_BUCKET is set but sccache is not installed" }
  $flagText += "`n# PLAINBROWSER-SCCACHE-BEGIN`ncc_wrapper=`"sccache`"`n# PLAINBROWSER-SCCACHE-END`n"
  if (-not $env:SCCACHE_REGION) { $env:SCCACHE_REGION = "auto" }
  & sccache --stop-server 2>$null | Out-Null
  & sccache --start-server
  Write-Host "compile cache: sccache, bucket $env:SCCACHE_BUCKET"
} else {
  Write-Host "compile cache: off"
}
Set-Content $flags -Value $flagText -NoNewline

# The Python modules build.py imports come from install-tools.ps1, which every stage runs first.

Push-Location $ucw
try {
  Write-Step "python build.py --ci -j $Jobs"
  & python build.py --ci -j $Jobs
  $rc = $LASTEXITCODE
} finally {
  Pop-Location
}
if ($useCache) { Write-Step "sccache statistics"; & sccache --show-stats; & sccache --stop-server | Out-Null }
Write-Step "build.py exited with $rc"
switch ($rc) {
  0 {
    Write-GitHubOutput finished true
    Get-ChildItem "$ucw\build\src\out\Default" -Filter "*.exe" | Where-Object { $_.Name -in "chrome.exe", "mini_installer.exe", "chromedriver.exe" } |
      ForEach-Object { Write-Host "  $($_.Name)  $([math]::Round($_.Length / 1MB)) MB" }
  }
  124 {
    Write-Host "ninja time slice ended; the next stage resumes from the saved tree"
    Write-GitHubOutput finished false
  }
  default { throw "build.py failed (exit $rc)" }
}
Write-DiskFree
# the step runs this script without -File, so its exit code is the last native command's: clear build.py's 124
$global:LASTEXITCODE = 0
