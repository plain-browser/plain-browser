#!/usr/bin/env python3
"""GN action: build the adblock FFI crate offline from vendored deps."""
import subprocess, sys, shutil
from pathlib import Path
crate, out = Path(sys.argv[1]), Path(sys.argv[2])
subprocess.check_call(["cargo", "build", "--release", "--offline", "--locked"], cwd=crate)
lib = next((crate / "target" / "release").glob("*draftway_adblock_ffi.*"), None)
for cand in (crate / "target" / "release").iterdir():
    if cand.suffix in (".a", ".lib") and "draftway_adblock_ffi" in cand.name:
        out.mkdir(parents=True, exist_ok=True)
        shutil.copy2(cand, out / "libdraftway_adblock_ffi.a"); break
else:
    sys.exit("staticlib not produced")
