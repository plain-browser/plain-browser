#!/usr/bin/env python3
"""Rename the product in Chromium's user-visible strings, keeping every translation.

Chromium's UI strings live in a few chromium_strings*.grd(p) files, and the translations in
*_chromium_strings_<lang>.xtb are looked up by a fingerprint of the English message text. Replacing
"Chromium" in the English text alone would therefore silently drop every translation of those messages.
So for each affected message this script computes the translation id before and after the change with the
tree's own grit, rewrites the English text, and moves each translation to the new id with the name replaced.

Usage: python apply-branding.py <chromium src dir> [--name "Plain Browser"] [--dry-run] [--force]
Idempotent: writes chrome/app/.plainbrowser_branded; a second run does nothing unless --force.
"""
import argparse, datetime, glob, io, os, re, sys

OLD = "Chromium"
# Legal and attribution phrases keep the upstream name.
KEEP = ["The Chromium Authors", "Chromium Authors", "Chromium open source", "Chromium OS", "ChromiumOS",
        "Chromium Embedded Framework"]
GRD_FILES = ["chrome/app/chromium_strings.grd", "chrome/app/settings_chromium_strings.grdp",
             "components/components_chromium_strings.grd", "extensions/strings/extensions_chromium_strings.grdp"]
XTB_DIRS = ["chrome/app/resources", "components/strings", "extensions/strings"]
# chrome/app/theme/chromium/BRANDING feeds exe version info and installer names. The file has CRLF line endings, so
# it is edited here by exact substring (without the line ending) instead of by a patch.
BRANDING_FILE = "chrome/app/theme/chromium/BRANDING"
BRANDING_EDITS = [
    ("COMPANY_FULLNAME=The Chromium Authors", "COMPANY_FULLNAME={name} contributors"),
    ("COMPANY_SHORTNAME=The Chromium Authors", "COMPANY_SHORTNAME={name} contributors"),
    ("PRODUCT_FULLNAME=Chromium", "PRODUCT_FULLNAME={name}"),
    ("PRODUCT_SHORTNAME=Chromium", "PRODUCT_SHORTNAME={name}"),
    ("PRODUCT_INSTALLER_FULLNAME=Chromium Installer", "PRODUCT_INSTALLER_FULLNAME={name} Installer"),
    ("PRODUCT_INSTALLER_SHORTNAME=Chromium Installer", "PRODUCT_INSTALLER_SHORTNAME={name} Installer"),
    ("COPYRIGHT=Copyright @LASTCHANGE_YEAR@ The Chromium Authors. All rights reserved.",
     "COPYRIGHT=Copyright @LASTCHANGE_YEAR@ The Chromium Authors and {name} contributors. All rights reserved."),
]
# The installer-built exe does not substitute @LASTCHANGE_YEAR@ in BRANDING, so it would show up literally.
BRANDING_YEAR = str(datetime.date.today().year)

# Attribution messages (publisher in the uninstall entry, about-page copyright) name the upstream authors as
# legal text. They keep that credit and add ours. Other languages fall back to the English text, because a
# machine-made translation of a legal line is worse than none; Polish is written out by hand.
ATTRIBUTION_EN = [
    ("The Chromium Authors. All rights reserved.", "The Chromium Authors and {name} contributors. All rights reserved."),
]
ATTRIBUTION_PL = [
    ("Autorzy Chromium. Wszelkie", "Autorzy Chromium i współtwórcy {name}. Wszelkie"),
]


def attribution_en(body, new):
    if body.strip() == "The Chromium Authors":
        return body.replace("The Chromium Authors", "%s contributors" % new)
    for o, n in ATTRIBUTION_EN:
        body = body.replace(o, n.format(name=new))
    return body


def attribution_pl(body, new):
    if body.strip() == "Autorzy Chromium":
        return body.replace("Autorzy Chromium", "Współtwórcy %s" % new)
    for o, n in ATTRIBUTION_PL:
        body = body.replace(o, n.format(name=new))
    return body


def rebrand(text, new):
    for i, k in enumerate(KEEP):
        text = text.replace(k, "\x00%d\x00" % i)
    text = text.replace(OLD, new)
    for i, k in enumerate(KEEP):
        text = text.replace("\x00%d\x00" % i, k)
    return text


def rebrand_markup(markup, new):
    """Replace only in text between tags, never in tag names or attributes.
    The link text of "... made possible by the <link>Chromium</link> open source project" names the
    upstream project and is kept. It is the text after the BEGIN_LINK_CHROMIUM placeholder: in the .grd the
    placeholder is <ph ...>code</ph>, in the .xtb it is <ph .../>, so the protected text follows its end."""
    parts = re.split(r"(<[^>]*>)", markup)
    out = []
    in_begin = protect_next = False
    for i, p in enumerate(parts):
        if i % 2:
            if "BEGIN_LINK_CHROMIUM" in p:
                if p.endswith("/>"):
                    protect_next = True
                else:
                    in_begin = True
            elif in_begin and p.startswith("</ph"):
                in_begin = False
                protect_next = True
            out.append(p)
        elif protect_next and p.strip():
            out.append(p)
            protect_next = False
        else:
            out.append(rebrand(p, new))
    return "".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("--name", default="Plain Browser")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--force", action="store_true")
    a = ap.parse_args()
    src, new = a.src, a.name
    marker = os.path.join(src, "chrome", "app", ".plainbrowser_branded")
    if os.path.exists(marker) and not a.force:
        print("already branded (%s); use --force to redo" % open(marker).read().strip())
        return 0
    sys.path.insert(0, os.path.join(src, "tools", "grit"))
    from grit import grd_reader

    def msg_id(frag):
        w = ('<?xml version="1.0" encoding="UTF-8"?><grit latest_public_release="0" current_release="1" base_dir=".">'
             '<release seq="1"><messages>' + frag + "</messages></release></grit>")
        root = grd_reader.Parse(io.StringIO(w), dir=src, debug=False, defines={})
        for n in root.ActiveDescendants():
            if n.name == "message":
                return str(n.GetCliques()[0].GetId())
        raise ValueError("no message parsed")

    idmap = {}  # old translation id -> new id
    attr_ids = set()  # ids of attribution messages, retranslated by hand
    new_files = {}
    total = changed = 0
    for rel in GRD_FILES:
        path = os.path.join(src, rel)
        text = open(path, encoding="utf-8", newline="").read()
        n_here = 0

        def sub(m):
            nonlocal total, changed, n_here
            total += 1
            head, body, tail = m.group(1), m.group(2), m.group(3)
            body_a = attribution_en(body, new)
            if OLD not in re.sub(r"<[^>]*>", "", body_a) and body_a == body:
                return m.group(0)
            new_body = rebrand_markup(body_a, new)
            if new_body == body:
                return m.group(0)
            changed += 1
            n_here += 1
            old_frag, new_frag = head + body + tail, head + new_body + tail
            if 'translateable="false"' not in head:
                oi, ni = msg_id(old_frag), msg_id(new_frag)
                idmap[oi] = ni
                if body_a != body:
                    attr_ids.add(oi)
            return new_frag

        out = re.sub(r"(<message\b[^>]*>)(.*?)(</message>)", sub, text, flags=re.S)
        new_files[path] = out
        print("%-52s messages changed: %d" % (rel, n_here))
    print("messages: %d seen, %d changed, %d translatable ids to remap" % (total, changed, len(idmap)))

    xtb_stats = {}
    for d in XTB_DIRS:
        for path in sorted(glob.glob(os.path.join(src, d, "*chromium_strings_*.xtb"))):
            text = open(path, encoding="utf-8", newline="").read()
            existing = set(re.findall(r'<translation id="(-?\d+)"', text))
            moved = collided = 0

            def tsub(m):
                nonlocal moved, collided
                oid, attrs, body = m.group(1), m.group(2), m.group(3)
                if oid not in idmap:
                    return m.group(0)
                nid = idmap[oid]
                if oid in attr_ids:
                    if not path.endswith("_pl.xtb"):
                        return m.group(0)
                    body = attribution_pl(body, new)
                if nid != oid and nid in existing:
                    collided += 1
                    return m.group(0)
                moved += 1
                return '<translation id="%s"%s>%s</translation>' % (nid, attrs, rebrand_markup(body, new))

            out = re.sub(r'<translation id="(-?\d+)"([^>]*)>(.*?)</translation>', tsub, text, flags=re.S)
            new_files[path] = out
            xtb_stats[path] = (moved, collided, len(existing))
    langs = len(xtb_stats)
    moved_pl = [v for k, v in xtb_stats.items() if k.endswith("chromium_strings_pl.xtb")]
    print("xtb files: %d; translations moved in pl: %s (moved, id collisions, total)" % (langs, moved_pl))
    coll = sum(v[1] for v in xtb_stats.values())
    if coll:
        print("WARNING: %d id collisions left unchanged" % coll)
    bpath = os.path.join(src, *BRANDING_FILE.split("/"))
    btext = open(bpath, encoding="utf-8", newline="").read()
    for old_s, new_s in BRANDING_EDITS:
        if old_s in btext:
            btext = btext.replace(old_s, new_s.format(name=new))
        elif new_s.format(name=new) not in btext:
            raise SystemExit("BRANDING: expected line not found: " + old_s)
    btext = btext.replace("@LASTCHANGE_YEAR@", BRANDING_YEAR)
    new_files[bpath] = btext
    print("BRANDING: %d lines edited" % len(BRANDING_EDITS))
    if a.dry_run:
        print("dry run: nothing written")
        return 0
    for path, out in new_files.items():
        with open(path, "w", encoding="utf-8", newline="") as f:
            f.write(out)
    with open(marker, "w") as f:
        f.write("%s (%d messages, %d translation ids)\n" % (new, changed, len(idmap)))
    print("written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
