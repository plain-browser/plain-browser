#!/usr/bin/env bash
# shot.sh <out.png>: console screenshot of PB-VIDEO
unraid="${PB_UNRAID:?set PB_UNRAID=user@host of the Unraid server}"
ssh -o BatchMode=yes -o LogLevel=ERROR "$unraid" 'virsh screenshot plainbrowser-video /tmp/pbv-s.png >/dev/null' && scp -q -o LogLevel=ERROR "$unraid:/tmp/pbv-s.png" "$1"
