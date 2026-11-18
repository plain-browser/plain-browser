import importlib.util, unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location("check_crates", Path(__file__).resolve().parent / "check-crates.py")
cc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cc)

R = "registry+https://github.com/rust-lang/crates.io-index"
A, B, C = "a" * 64, "b" * 64, "c" * 64


def lock(*pkgs):
    out = ["version = 4", ""]
    for name, version, source, checksum in pkgs:
        out += ["[[package]]", f'name = "{name}"', f'version = "{version}"']
        if source:
            out.append(f'source = "{source}"')
        if checksum:
            out.append(f'checksum = "{checksum}"')
        out.append("")
    return "\n".join(out)


BEFORE = lock(("serde", "1.0.1", R, None), ("plainbrowser", "0.1.0", None, None))
AFTER = lock(("serde", "1.0.1", R, A), ("plainbrowser", "0.1.0", None, None), ("seahash", "4.1.0", R, B), ("adblock", "0.13.3", R, C))
PINS = f"# comment\nseahash 4.1.0 {B}\nadblock 0.13.3 {C}  # trailing\n"


class CheckCrates(unittest.TestCase):
    def test_added_are_the_new_registry_crates_only(self):
        new = cc.added(cc.parse_lock(BEFORE), cc.parse_lock(AFTER))
        self.assertEqual(new, {("seahash", "4.1.0"): B, ("adblock", "0.13.3"): C})

    def test_match(self):
        code, out = cc.check(BEFORE, AFTER, PINS)
        self.assertEqual(code, 0)
        self.assertIn("2 added crates match", out[-1])

    def test_changed_checksum_fails(self):
        code, out = cc.check(BEFORE, AFTER, PINS.replace(B, "d" * 64))
        self.assertEqual(code, 1)
        self.assertTrue(any("seahash 4.1.0" in l and "differs" in l for l in out))

    def test_unlisted_crate_fails(self):
        code, out = cc.check(BEFORE, AFTER, f"adblock 0.13.3 {C}\n")
        self.assertEqual(code, 1)
        self.assertTrue(any("seahash 4.1.0" in l and "not in" in l for l in out))

    def test_new_version_of_a_listed_crate_fails(self):
        after = AFTER.replace('version = "4.1.0"', 'version = "4.1.1"')
        code, _ = cc.check(BEFORE, after, PINS)
        self.assertEqual(code, 1)

    def test_empty_pins_warn_and_list(self):
        code, out = cc.check(BEFORE, AFTER, "# nothing yet\n")
        self.assertEqual(code, 0)
        self.assertTrue(out[0].startswith("::warning::"))
        self.assertIn(f"seahash 4.1.0 {B}", out)

    def test_pinned_but_not_added_is_a_note(self):
        code, out = cc.check(BEFORE, AFTER, PINS + f"extra 1.0.0 {A}\n")
        self.assertEqual(code, 0)
        self.assertTrue(any(l.startswith("note:") and "extra 1.0.0" in l for l in out))

    def test_bad_pin_line_is_an_error(self):
        with self.assertRaises(ValueError):
            cc.parse_pins("seahash 4.1.0 nothex\n")

    def test_git_and_path_sources_are_not_pinned(self):
        after = lock(("serde", "1.0.1", R, None), ("gitcrate", "1.0.0", "git+https://example.com/x", C))
        self.assertEqual(cc.added(cc.parse_lock(BEFORE), cc.parse_lock(after)), {})


if __name__ == "__main__":
    unittest.main()
