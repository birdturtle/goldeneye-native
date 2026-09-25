"""ROM-free checks for incomplete background extraction and stale generated blobs."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "check_bg_assets.py"
SPEC = importlib.util.spec_from_file_location("check_bg_assets", SCRIPT)
CHECK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK)


class BackgroundAssetsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for directory in ("scripts", "assets/obseg/bg", "assets/obseg"):
            (self.root / directory).mkdir(parents=True, exist_ok=True)
        (self.root / "scripts/filelist.u.csv").write_text(
            "0x100,4,assets/obseg/bg/bg_ame_all_p.bin,0,1\n"
            "0x200,0,assets/obseg/bg/bg_cut_all_p.bin,0,1\n",
            encoding="utf-8")
        (self.root / "assets/obseg/ob_seg.s").write_text(
            "bg_file_seg bg_ame_all_p_seg, bg_ame_all_p\n",
            encoding="utf-8")
        (self.root / "assets/obseg/file_resource_table.inc.c").write_text(
            '{BG_AME_ALL_P, "bg/bg_ame_all_p.seg", &bg_ame_all_p_seg},\n',
            encoding="utf-8")

    def test_missing_and_short_live_backgrounds_are_rejected(self):
        self.assertIn("missing or incomplete", CHECK.background_errors(self.root)[0])
        asset = self.root / "assets/obseg/bg/bg_ame_all_p.bin"
        asset.write_bytes(b"abc")
        self.assertIn("missing or incomplete", CHECK.background_errors(self.root)[0])
        asset.write_bytes(b"abcd")
        self.assertEqual([], CHECK.background_errors(self.root))

    def test_generated_size_table_must_match_existing_background(self):
        (self.root / "assets/obseg/bg/bg_ame_all_p.bin").write_bytes(b"abcd")
        header = self.root / "assets/obseg/ge_obseg_bg_sizes.h"
        blobs = self.root / "assets/obseg/ge_obseg_blobs.c"
        header.write_text("{ bg_ame_all_p_seg, 4u },\n", encoding="utf-8")
        blobs.write_text("unsigned char bg_ame_all_p_seg[4] = {0};\n", encoding="utf-8")
        self.assertEqual([], CHECK.background_errors(self.root, generated=True))
        header.write_text("{ bg_ame_all_p_seg, 3u },\n", encoding="utf-8")
        self.assertIn("wrong size", CHECK.background_errors(self.root, generated=True)[0])

    def test_live_background_cannot_register_cut_level_stub(self):
        (self.root / "assets/obseg/bg/bg_ame_all_p.bin").write_bytes(b"abcd")
        (self.root / "assets/obseg/ge_obseg_bg_sizes.h").write_text(
            "{ bg_ame_all_p_seg, 4u },\n", encoding="utf-8")
        (self.root / "assets/obseg/ge_obseg_blobs.c").write_text(
            "unsigned char bg_ame_all_p_seg[4] = {0};\n", encoding="utf-8")
        table = self.root / "assets/obseg/file_resource_table.inc.c"
        table.write_text(
            '{BG_AME_ALL_P, "bg/bg_ame_all_p.seg", &bg_imp_all_p_seg},\n',
            encoding="utf-8")
        self.assertIn("registers bg_imp_all_p_seg; expected bg_ame_all_p_seg",
                      CHECK.background_errors(self.root, generated=True)[0])

    def test_generator_refuses_incomplete_input_without_replacing_blobs(self):
        blobs = self.root / "assets/obseg/ge_obseg_blobs.c"
        blobs.write_text("existing generated output\n", encoding="utf-8")
        result = subprocess.run(
            [sys.executable, str(SCRIPT.parent / "gen_obseg_blobs.py")],
            cwd=self.root, capture_output=True, text=True, check=False)
        self.assertNotEqual(0, result.returncode)
        self.assertIn("bg_ame_all_p.bin", result.stderr)
        self.assertEqual("existing generated output\n", blobs.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
