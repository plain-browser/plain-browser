#!/usr/bin/env bash
# shot.sh <out.png>: console screenshot of DW-VIDEO
ssh -o BatchMode=yes -o LogLevel=ERROR root@192.168.1.5 'virsh screenshot plainbrowser-video /tmp/pbv-s.png >/dev/null' && scp -q -o LogLevel=ERROR root@192.168.1.5:/tmp/pbv-s.png "$1"
