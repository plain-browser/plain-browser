#!/usr/bin/env bash
# gexec.sh '<powershell command>' [wait-s]: run a command in DW-VIDEO through the QEMU guest agent and print its output
unraid="${PB_UNRAID:?set PB_UNRAID=user@host of the Unraid server}"
cmd=$(python3 -I -c 'import json,sys; print(json.dumps({"execute":"guest-exec","arguments":{"path":"powershell.exe","arg":["-NoProfile","-Command",sys.argv[1]],"capture-output":True}}))' "$1")
pid=$(ssh -o BatchMode=yes -o LogLevel=ERROR "$unraid" "virsh qemu-agent-command plainbrowser-video '$cmd'" | python3 -I -c 'import json,sys; print(json.load(sys.stdin)["return"]["pid"])')
sleep "${2:-6}"
ssh -o BatchMode=yes -o LogLevel=ERROR "$unraid" "virsh qemu-agent-command plainbrowser-video '{\"execute\":\"guest-exec-status\",\"arguments\":{\"pid\":$pid}}'" | python3 -I -c '
import json,sys,base64; d=json.load(sys.stdin)["return"]
print("exited:",d.get("exited"),"code:",d.get("exitcode"))
for k in ("out-data","err-data"):
    if d.get(k): print(base64.b64decode(d[k]).decode("utf-8","replace")[:3000])'
