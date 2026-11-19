#!/usr/bin/env bash
# up.sh <file>...: copy shot scripts to DW-VIDEO with a UTF-8 BOM (Windows PowerShell 5 reads BOM-less files as ANSI).
vm="${PB_VIDEO_VM:?set PB_VIDEO_VM=user@host of the DW-VIDEO VM}"
for f in "$@"; do t=$(mktemp); if head -c3 "$f" | grep -q $'\xef\xbb\xbf'; then cp "$f" "$t"; else { printf '\xef\xbb\xbf'; cat "$f"; } > "$t"; fi
scp -q -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR "$t" "$vm:C:/PlainBrowser/shots/$(basename "$f")"; rm -f "$t"; done
