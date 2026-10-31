#!/usr/bin/env python3
"""Repair a tree prepared before patches/upstream-revert.list kept GetEnterpriseProfileId (run 41011279686, tree-5).

ungoogled's remove-unused-preferences-fields.patch removed the definition of GetEnterpriseProfileId from
chrome/browser/enterprise/signals/context_info_fetcher.cc, while our filter dropped the hunk that removes its call,
so ninja stopped on "use of undeclared identifier 'GetEnterpriseProfileId'". A fresh prepare-tree.ps1 no longer does
this; this puts the definition back into an already prepared tree. Does nothing when the definition is there.

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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--work", default=r"C:\pb")
    args = ap.parse_args()
    path = Path(args.work) / "ucw" / "build" / "src" / "chrome" / "browser" / "enterprise" / "signals" / "context_info_fetcher.cc"
    if not path.exists():
        sys.exit(f"{path} not found")
    text = path.read_bytes().decode("utf-8")
    try:
        fixed = fix(text)
    except ValueError as e:
        sys.exit(f"{path}: {e}")
    if fixed is None:
        print("context_info_fetcher.cc already defines GetEnterpriseProfileId, nothing to do")
        return
    path.write_bytes(fixed.encode("utf-8"))
    print(f"restored GetEnterpriseProfileId in {path}")
    for i, line in enumerate(fixed.splitlines(), 1):
        if "GetEnterpriseProfileId" in line:
            print(f"    {i}: {line.strip()}")


if __name__ == "__main__":
    main()
