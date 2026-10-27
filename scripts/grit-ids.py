#!/usr/bin/env python3
# Copyright Plain Browser contributors. MPL-2.0.
"""Translation ids for our .grdp string files, computed by grit itself.

A translation in an .xtb file is keyed by grit's fingerprint of the English
message (MD5-based, includes the placeholder names and the meaning attribute),
so the ids cannot be written by hand. This script parses a .grdp with grit and
prints name, id and the text the id is computed from; with --xtb it also checks
a translation file against the .grdp: unknown ids, missing translations and
placeholders that do not match.

grit is pure Python and lives in the Chromium tree (tools/grit); copy that
directory anywhere and pass it with --grit.

  scripts/grit-ids.py --grit <chromium>/tools/grit \
      src/plainbrowser/shields/resources/plainbrowser_shields_strings.grdp \
      --xtb src/plainbrowser/shields/resources/plainbrowser_shields_strings_pl.xtb
"""
import argparse
import os
import re
import sys
import tempfile
import xml.etree.ElementTree as ET


def message_ids(grit_dir, grdp):
    sys.path.insert(0, os.path.abspath(grit_dir))
    from grit import grd_reader
    from grit.node import message

    grd = ('<?xml version="1.0" encoding="UTF-8"?>\n'
           '<grit latest_public_release="0" current_release="1">\n'
           '  <outputs><output filename="x.h" type="rc_header"/></outputs>\n'
           '  <release seq="1"><messages>\n'
           f'    <part file="{os.path.abspath(grdp)}" />\n'
           '  </messages></release>\n'
           '</grit>\n')
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, 'wrapper.grd')
        with open(path, 'w', encoding='utf-8') as f:
            f.write(grd)
        root = grd_reader.Parse(path, dir=tmp)
        out = []
        for node in root.ActiveDescendants():
            if isinstance(node, message.MessageNode):
                msg = node.GetCliques()[0].GetMessage()
                phs = sorted(p.GetPresentation() for p in msg.GetPlaceholders())
                out.append((node.attrs['name'], msg.GetId(),
                            msg.GetPresentableContent(), phs))
        return out


def check_xtb(messages, xtb):
    by_id = {m[1]: m for m in messages}
    seen = set()
    errors = 0
    for t in ET.parse(xtb).getroot().iter('translation'):
        tid = t.get('id')
        if tid not in by_id:
            print(f'{xtb}: unknown id {tid}: {"".join(t.itertext())[:60]!r}')
            errors += 1
            continue
        if tid in seen:
            print(f'{xtb}: id {tid} translated twice (grit asserts on this)')
            errors += 1
        seen.add(tid)
        phs = sorted(ph.get('name') for ph in t.iter('ph'))
        if phs != by_id[tid][3]:
            print(f'{xtb}: {by_id[tid][0]}: placeholders {phs}, '
                  f'expected {by_id[tid][3]}')
            errors += 1
    for name, mid, _, _ in messages:
        if mid not in seen:
            print(f'{xtb}: no translation for {name} ({mid})')
            errors += 1
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--grit', required=True,
                        help='Chromium tools/grit directory (or a copy)')
    parser.add_argument('grdp')
    parser.add_argument('--xtb', action='append', default=[])
    args = parser.parse_args()
    messages = message_ids(args.grit, args.grdp)
    if not args.xtb:
        for name, mid, text, _ in messages:
            print(f'{name}\t{mid}\t{re.sub(r"\s+", " ", text)}')
        return 0
    errors = sum(check_xtb(messages, x) for x in args.xtb)
    print(f'{len(messages)} messages, {errors} problem(s)')
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
