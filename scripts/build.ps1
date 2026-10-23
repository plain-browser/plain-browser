<# Build Draftway alpha on a Windows 11 builder (run from "Developer PowerShell for VS", as admin).
   Needs: VS 2022 w/ Win11 SDK, Python 3.11+, Git, Rust (rustup), ~300 GB free, 64 GB RAM recommended. #>
param([string]$Work = "C:\dw-build")
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$ver = (Select-String -Path "$root\versions.toml" -Pattern 'ungoogled_chromium_windows\s*=\s*"([^"]+)"').Matches[0].Groups[1].Value
if ($ver -like "TODO*") { throw "Set ungoogled_chromium_windows tag in versions.toml first." }
New-Item -ItemType Directory -Force $Work | Out-Null
if (-not (Test-Path "$Work\ucw")) { git clone --recurse-submodules https://github.com/ungoogled-software/ungoogled-chromium-windows.git "$Work\ucw" }
Push-Location "$Work\ucw"; git fetch --tags; git checkout $ver; git submodule update --init --recursive; Pop-Location
# Source download + stock ungoogled patching happens inside build.py; run the Draftway layer after the tree exists.
Push-Location "$Work\ucw"
python build.py --ci          # downloads Chromium, applies ungoogled patches, builds (hours)
Pop-Location
python "$root\scripts\prepare.py" "$Work\ucw" "$Work\ucw\build\src"
Push-Location "$Work\ucw\build\src"
cargo vendor --manifest-path draftway/adblock/rs/Cargo.toml draftway/adblock/rs/vendor | Out-Null
Pop-Location
Write-Host "Stock build done and Draftway layer staged. Next: generate branding/build patches against this tree, rebuild, run package.py."
