#!/usr/bin/env bash
# frames.sh <name> <t1,t2,...>: extract frames (seconds) from Videos\<name>.mkv on PB-VIDEO into scratchpad/frames/<name>-<t>.png at 960 wide
vm="${PB_VIDEO_VM:?set PB_VIDEO_VM=user@host of the PB-VIDEO VM}"
n="$1"; ts="$2"; S="${S:-$(mktemp -d)}"; mkdir -p $S/frames
ssh -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR "$vm" "
\$ff = (Get-ChildItem \"\$env:LOCALAPPDATA\Microsoft\WinGet\Packages\Gyan.FFmpeg*\*\bin\" | Select-Object -First 1).FullName
\$v = 'C:\Users\builder\Videos\\$n.mkv'; New-Item -ItemType Directory -Force C:\PlainBrowser\frames | Out-Null
& \"\$ff\ffprobe.exe\" -v error -show_entries format=duration:stream=width,height,r_frame_rate -of compact \$v
foreach (\$t in '$ts'.Split(',')) { & \"\$ff\ffmpeg.exe\" -v error -y -ss \$t -i \$v -frames:v 1 -vf scale=960:-1 \"C:\PlainBrowser\frames\\$n-\$t.png\" }"
for t in ${ts//,/ }; do scp -q -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR "$vm:C:/PlainBrowser/frames/$n-$t.png" $S/frames/; done
