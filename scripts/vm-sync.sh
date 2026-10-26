#!/usr/bin/env bash
# Copy this checkout (tracked + untracked, minus ignored files) to the build VM as C:\dw-build\repo.
# The VM has no git clone of the repo; scripts there read C:\dw-build\repo (apply-plainbrowser.ps1 -Repo).
# Usage: scripts/vm-sync.sh [user@host]   (default builder@192.168.1.33, key ~/.ssh/id_ed25519)
set -euo pipefail
host="${1:-builder@192.168.1.33}"
root="$(cd "$(dirname "$0")/.." && pwd)"
tmp="$(mktemp)"; trap 'rm -f "$tmp"' EXIT
( cd "$root" && git ls-files -z --cached --others --exclude-standard | grep -z -v -E '^(website/|dist/|infra/.*/node_modules/)' | tar czf "$tmp" --null -T - )
scp -q "$tmp" "$host:C:/dw-build/repo.tgz"
ssh "$host" 'powershell -NoProfile -Command "if (Test-Path C:\dw-build\repo) { Remove-Item C:\dw-build\repo -Recurse -Force }; New-Item -ItemType Directory C:\dw-build\repo | Out-Null; tar xzf C:\dw-build\repo.tgz -C C:\dw-build\repo; (Get-ChildItem C:\dw-build\repo -Recurse -File | Measure-Object).Count"'
