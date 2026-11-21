#!/usr/bin/env python3
"""Repair a tree that was prepared from older patches than the revision that resumes it (resume_run in build-windows.yml).

1. GetEnterpriseProfileId (run 41011279686, tree-5).

ungoogled's remove-unused-preferences-fields.patch removed the definition of GetEnterpriseProfileId from
chrome/browser/enterprise/signals/context_info_fetcher.cc, while our filter dropped the hunk that removes its call,
so ninja stopped on "use of undeclared identifier 'GetEnterpriseProfileId'". A fresh prepare-tree.ps1 no longer does
this; this puts the definition back into an already prepared tree. Does nothing when the definition is there.

2. Settings lint (run 42287983857, tree-4; tag v0.2.0-beta.5): eslint stopped build-5 on two Plain Browser files.
   plainbrowser_shields_exceptions.ts: `!!pref.value` on a boolean; security_page.ts: the `$` interface still listed
   passwordsLeakToggle (ungoogled removes that element from the template) and imported its type. Both are fixed in
   patches/60-ui/0009 and 70-safebrowsing/0005 now; this applies the same edits to the resumed tree (the settings
   resources are built in stage 5, after the point where the tree was saved).

3. chrome_command_ids.h (run 44745294426, tree-6; tag v0.2.5-beta.3): rc.exe stopped build-7 with RC1012 "mismatched
   parenthesis" at line 105. rc.exe does not know BUILDFLAG(), so ungoogled-chromium-windows'
   windows-fix-command-ids.patch replaces those #if lines with constants; at commit cde653e of its tag 158.0.8127.36-1.1
   three hunks were missing. Upstream restored them in 76a692e and moved the tag there (versions.toml pins it now);
   this makes the same three edits in a tree prepared from the old commit.

4. version.h: the tree was compiled with the version of the run that made it; copy the one of this revision (it is
   included by a handful of files, ninja rebuilds only those).

usage: hotfix-tree.py [--work C:\\pb]
"""
import argparse, re, sys
from pathlib import Path

DEFINITION = """std::optional<std::string> GetEnterpriseProfileId(Profile* profile) {
  auto* profile_id_service =
      enterprise::ProfileIdServiceFactory::GetForProfile(profile);
  if (profile_id_service) {
    return profile_id_service->GetProfileId();
  }
  return std::nullopt;
}

"""


def fix(text):
    """Returns the repaired text, or None when nothing is to do. Raises ValueError when the file is not what we expect."""
    if re.search(r"GetEnterpriseProfileId\(Profile\* profile\)\s*\{", text):
        return None
    if "GetEnterpriseProfileId(profile)" not in text:
        raise ValueError("the file neither defines nor calls GetEnterpriseProfileId: the hotfix does not apply")
    nl = "\r\n" if "\r\n" in text else "\n"
    m = re.search(r"namespace \{" + nl + nl, text)  # the anonymous namespace
    if not m:
        raise ValueError("anonymous namespace not found")
    return text[:m.end()] + DEFINITION.replace("\n", nl) + text[m.end():]


def edit_file(path, edits, forbidden):
    """edits: [(old, new), ...], each applied when `old` is there and skipped when it is not (the tree was prepared from
    the fixed patches already). Afterwards none of the `forbidden` strings may remain."""
    text = path.read_bytes().decode("utf-8")
    nl = "\r\n" if "\r\n" in text else "\n"
    changed = False
    for old, new in edits:
        old, new = old.replace("\n", nl), new.replace("\n", nl)
        if old in text:
            text = text.replace(old, new, 1)
            changed = True
            print(f"{path.name}: edited {old.strip()[:70]!r}")
    for f in forbidden:
        if f in text:
            raise SystemExit(f"{path}: still contains {f!r}: the hotfix does not fit this tree")
    if changed:
        path.write_bytes(text.encode("utf-8"))
    else:
        print(f"{path.name}: nothing to do")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--work", default=r"C:\pb")
    args = ap.parse_args()
    src = Path(args.work) / "ucw" / "build" / "src"
    repo = Path(__file__).resolve().parent.parent.parent

    path = src / "chrome" / "browser" / "enterprise" / "signals" / "context_info_fetcher.cc"
    if not path.exists():
        sys.exit(f"{path} not found")
    text = path.read_bytes().decode("utf-8")
    try:
        fixed = fix(text)
    except ValueError as e:
        sys.exit(f"{path}: {e}")
    if fixed is None:
        print("context_info_fetcher.cc already defines GetEnterpriseProfileId, nothing to do")
    else:
        path.write_bytes(fixed.encode("utf-8"))
        print(f"restored GetEnterpriseProfileId in {path}")

    settings = src / "chrome" / "browser" / "resources" / "settings" / "privacy_page"
    edit_file(settings / "plainbrowser_shields_exceptions.ts",
              [("this.listUpdatesEnabled_ = !!pref.value;", "this.listUpdatesEnabled_ = pref.value;")],
              ["!!pref.value"])
    edit_file(settings / "security" / "security_page.ts", [
        ("import type {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';\n", ""),
        ("    passwordsLeakToggle: SettingsToggleButtonElement,\n", ""),
    ], ["passwordsLeakToggle", "SettingsToggleButtonElement"])

    edit_file(src / "chrome" / "app" / "chrome_command_ids.h", [
        ("#if BUILDFLAG(IS_CHROMEOS)\n// Move window to other user commands\n", "#if (0)\n// Move window to other user commands\n"),
        ("// Context menu items for media toolbar button\n#if BUILDFLAG(GOOGLE_CHROME_BRANDING)\n",
         "// Context menu items for media toolbar button\n#if (0)\n"),
        ("#if BUILDFLAG(IS_CHROMEOS)\n// Quick Answers context menu items.\n", "#if (0)\n// Quick Answers context menu items.\n"),
    ], ["#if BUILDFLAG("])

    version_h = src / "plainbrowser" / "common" / "version.h"
    want = (repo / "src" / "plainbrowser" / "common" / "version.h").read_bytes()
    if version_h.read_bytes() != want:
        version_h.write_bytes(want)
        print("version.h replaced by this revision's")
    else:
        print("version.h is current")


if __name__ == "__main__":
    main()
