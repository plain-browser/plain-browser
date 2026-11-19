#!/usr/bin/env bash
# runshot.sh <shot-name> [timeout-s]: run C:\PlainBrowser\shots\<name>.ps1 in the desktop session of DW-VIDEO and wait for it.
vm="${PB_VIDEO_VM:?set PB_VIDEO_VM=user@host of the DW-VIDEO VM}"
n="$1"; t="${2:-300}"
ssh -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR "$vm" "
\$log='C:\PlainBrowser\shots\shot.log'; \$start = if (Test-Path \$log) { (Get-Content \$log).Count } else { 0 }
'$n' | Set-Content C:\PlainBrowser\shots\current.txt; Start-ScheduledTask -TaskName PBShot
\$i=0; while (\$i -lt $t) { Start-Sleep 2; \$i+=2; if (Test-Path \$log) { \$new = Get-Content \$log | Select-Object -Skip \$start; if (\$new -match '=== end $n') { break } } }
Get-Content \$log | Select-Object -Skip \$start | ForEach-Object { \$_.Substring(0, [Math]::Min(200, \$_.Length)) }"
