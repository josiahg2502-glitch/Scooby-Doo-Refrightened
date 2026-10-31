import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from PIL import Image, ImageDraw

SCRIPT = Path(__file__).with_name("make-button-icons.py")
SPEC = importlib.util.spec_from_file_location("button_icons", SCRIPT)
icons = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(icons)


def sample_atlas(size=64):
    image = Image.new("RGBA", (size, size))
    draw = ImageDraw.Draw(image)
    half, padding = size // 2, size // 8
    for (x, y), color in zip(
        ((0, 0), (half, 0), (0, half), (half, half)),
        ((255, 255, 0), (128, 128, 128), (0, 255, 0), (255, 0, 0)),
    ):
        draw.rectangle((x + padding, y + padding, x + half - padding - 1, y + half - padding - 1),
                       fill=(*color, 255))
    return image


class ButtonIconTests(unittest.TestCase):
    def test_variant_contract(self):
        self.assertEqual(len(icons.style_variants()), 6)
        self.assertEqual(len(icons.style_variants(all_bindings=True)), 768)
        variants = icons.style_variants(["xbox"], all_bindings=True)
        self.assertEqual(len(variants), 256)
        mappings = set()
        for name, buttons in variants.items():
            mapping = "".join(buttons[slot].rsplit("_", 1)[1].upper() for slot in "ABXY")
            mappings.add(mapping)
            if name == "xbox":
                self.assertEqual(mapping, "ABXY")
            elif name == "xbox_original":
                self.assertEqual(mapping, "AXBY")
            else:
                self.assertEqual(name, "xbox_" + mapping)
        self.assertEqual(len(mappings), 256)

    def test_reuse_preserves_identity_and_unchanged_quadrants(self):
        for size in (64, 256, 1024):
            with self.subTest(size=size):
                base = sample_atlas(size)
                boxes = icons.regions(base)
                identity = icons.remap_atlas(base, boxes, dict(zip("ABXY", "ABXY")))
                self.assertEqual(identity.tobytes(), base.tobytes())
                swapped = icons.remap_atlas(base, boxes, dict(zip("ABXY", "AXBY")))
                self.assertEqual(swapped.size, base.size)
                half = size // 2
                for quadrant in ((0, 0, half, half), (0, half, half, size)):
                    self.assertEqual(swapped.crop(quadrant).tobytes(), base.crop(quadrant).tobytes())
                for slot, physical in (("X", "B"), ("B", "X")):
                    self.assertEqual(swapped.crop(boxes[slot]).tobytes(), base.crop(boxes[physical]).tobytes())

    def test_local_cli_needs_no_art_pack_or_imagemagick(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            atlas = root / "local.png"
            sample_atlas().save(atlas)
            command = [sys.executable, str(SCRIPT), "--atlas", str(atlas),
                       "--out", str(root / "output"), "--reuse-atlas",
                       "--families", "xbox", "--all-bindings"]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(len(list((root / "output").glob("*/" + icons.ATLAS))), 256)
            with Image.open(root / "output" / "xbox" / icons.ATLAS) as identity:
                self.assertEqual(identity.tobytes(), sample_atlas().tobytes())
            with Image.open(root / "output" / "xbox_original" / icons.ATLAS) as swapped:
                self.assertEqual(swapped.getpixel((40, 8)), (255, 0, 0, 255))
                self.assertEqual(swapped.getpixel((40, 40)), (128, 128, 128, 255))
            invalid = subprocess.run(command + ["--families", "playstation"], capture_output=True, text=True)
            self.assertNotEqual(invalid.returncode, 0)
            self.assertIn("--reuse-atlas requires --families xbox", invalid.stderr)

    def test_invalid_atlas_is_reported(self):
        with self.assertRaisesRegex(ValueError, "divide evenly"):
            icons.regions(Image.new("RGBA", (63, 64)))
        with self.assertRaisesRegex(ValueError, "no Y button"):
            icons.regions(Image.new("RGBA", (64, 64)))


if __name__ == "__main__":
    unittest.main()
