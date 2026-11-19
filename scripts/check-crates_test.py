import importlib.util, tempfile, unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location("check_crates", Path(__file__).resolve().parent / "check-crates.py")
cc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cc)


def crate(vendor, name, version, files=None):
    d = vendor / name
    d.mkdir(parents=True)
    (d / "Cargo.toml").write_text(f'[package]\nname = "x"\nversion = "{version}"\n\n[dependencies]\nversion = "9.9.9"\n')
    for rel, content in (files or {"src/lib.rs": "pub fn f() {}\n"}).items():
        (d / rel).parent.mkdir(parents=True, exist_ok=True)
        (d / rel).write_text(content)
    return d


class CheckCrates(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.vendor = Path(self.tmp.name)
        crate(self.vendor, "serde-v1", "1.0.1")                       # Chromium's own, there before vendoring
        self.before = {"serde-v1"}
        self.a = crate(self.vendor, "adblock-v0_13", "0.13.3")
        self.b = crate(self.vendor, "seahash-v4", "4.1.0", {"src/lib.rs": "x\n", "LICENSE": "MIT\n"})

    def tearDown(self):
        self.tmp.cleanup()

    def new(self):
        return cc.added(self.vendor, self.before)

    def pins(self):
        return "# c\n" + "".join(f"{n} {v} {h}  # trailing\n" for n, (v, h) in sorted(self.new().items()))

    def test_added_are_the_new_directories_only(self):
        self.assertEqual(sorted(self.new()), ["adblock-v0_13", "seahash-v4"])
        self.assertEqual(self.new()["adblock-v0_13"][0], "0.13.3")  # the [package] version, not a dependency's

    def test_tree_hash_is_stable_and_sees_content_names_and_new_files(self):
        h = cc.tree_hash(self.a)
        self.assertEqual(h, cc.tree_hash(self.a))
        (self.a / "src" / "lib.rs").write_text("pub fn g() {}\n")
        self.assertNotEqual(h, cc.tree_hash(self.a))
        h2 = cc.tree_hash(self.a)
        (self.a / "build.rs").write_text("fn main() {}\n")
        self.assertNotEqual(h2, cc.tree_hash(self.a))

    def test_match(self):
        code, out = cc.check(self.new(), self.pins())
        self.assertEqual(code, 0)
        self.assertIn("2 added crates match", out[-1])

    def test_changed_content_fails(self):
        pins = self.pins()
        (self.a / "src" / "lib.rs").write_text("pub fn evil() {}\n")
        code, out = cc.check(self.new(), pins)
        self.assertEqual(code, 1)
        self.assertTrue(any("adblock-v0_13" in l and "differs" in l for l in out))

    def test_new_version_fails(self):
        pins = self.pins()
        (self.b / "Cargo.toml").write_text('[package]\nversion = "4.1.1"\n')
        code, _ = cc.check(self.new(), pins)
        self.assertEqual(code, 1)

    def test_unlisted_crate_fails(self):
        crate(self.vendor, "extra-v1", "1.0.0")
        code, out = cc.check(self.new(), self.pins().replace(next(l for l in self.pins().splitlines() if l.startswith("extra")), ""))
        self.assertEqual(code, 1)
        self.assertTrue(any("extra-v1" in l and "not in" in l for l in out))

    def test_empty_pins_warn_and_list(self):
        code, out = cc.check(self.new(), "# nothing yet\n")
        self.assertEqual(code, 0)
        self.assertTrue(out[0].startswith("::warning::"))
        self.assertTrue(any(l.startswith("seahash-v4 4.1.0 ") for l in out))

    def test_pinned_but_not_added_is_a_note(self):
        code, out = cc.check(self.new(), self.pins() + "gone-v1 1.0.0 " + "a" * 64 + "\n")
        self.assertEqual(code, 0)
        self.assertTrue(any(l.startswith("note:") and "gone-v1" in l for l in out))

    def test_bad_pin_line_is_an_error(self):
        with self.assertRaises(ValueError):
            cc.parse_pins("seahash-v4 4.1.0 nothex\n")


if __name__ == "__main__":
    unittest.main()
