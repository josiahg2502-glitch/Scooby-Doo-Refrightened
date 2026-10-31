"""Builds the per-controller button prompt textures (Play/UserData/button_icons/<xbox|playstation|nintendo>).

The game's GameCube button picture (one atlas: Y, X, A, B) is redrawn with the matching buttons from Kenney's Input
Prompts (CC0, tools/button-art), each in its own spot so the game's texture coordinates still pick the right one, and
saved at the HD pack's size under the game texture's name. Aurora loads the set for the controller in use
(texture_replacement.cpp, hle/input/pad.cpp). GameCube buttons map by position: A south, B east, X west, Y north.
Xbox-original variants swap B/X prompts to match the original game's actions.

    python tools/make-button-icons.py [--out Play/UserData/button_icons]
Needs ImageMagick (magick) to render the SVGs, and the HD pack in texture_pack/ScoobyHD for the layout.
"""
import argparse
import os
import subprocess
import tempfile

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "tools", "button-art")
ATLAS = "tex1_64x64_f05a599cb8feb028_14.png"
STYLES = {
    "xbox": {"A": "xbox_button_color_a", "B": "xbox_button_color_b", "X": "xbox_button_color_x", "Y": "xbox_button_color_y"},
    "playstation": {"A": "playstation_button_color_cross", "B": "playstation_button_color_circle",
                    "X": "playstation_button_color_square", "Y": "playstation_button_color_triangle"},
    "nintendo": {"A": "switch_button_b", "B": "switch_button_a", "X": "switch_button_y", "Y": "switch_button_x"},
}

def style_variants():
    styles = dict(STYLES)
    for style, buttons in STYLES.items():
        variant = "xbox_original" if style == "xbox" else style + "_xbox"
        styles[variant] = dict(buttons, B=buttons["X"], X=buttons["B"])
    return styles


def regions(img):
    """Bounding boxes of the atlas's four buttons, by quadrant: Y top-left, X top-right, A bottom-left, B bottom-right."""
    a = img.getchannel("A").point(lambda v: 255 if v > 128 else 0)
    w, h = img.size
    out = {}
    for name, (x0, y0) in {"Y": (0, 0), "X": (w // 2, 0), "A": (0, h // 2), "B": (w // 2, h // 2)}.items():
        box = a.crop((x0, y0, x0 + w // 2, y0 + h // 2)).getbbox()
        assert box, f"no {name} button in the atlas"
        out[name] = (x0 + box[0], y0 + box[1], x0 + box[2], y0 + box[3])
    return out


def render_svg(svg, size, tmp, dark_symbol):
    png = os.path.join(tmp, os.path.basename(svg) + f".{size}.png")
    subprocess.run(["magick", "-background", "none", "-density", "600", svg, "-resize", f"{size}x{size}", png], check=True)
    icon = Image.open(png).convert("RGBA")
    # The symbol is cut out of the button: fill what the outside can't reach (white, dark on Nintendo's white buttons).
    outside = Image.new("L", (icon.width + 2, icon.height + 2), 0)
    outside.paste(icon.getchannel("A").point(lambda v: 255 if v > 32 else 0), (1, 1))
    ImageDraw.floodfill(outside, (0, 0), 128)
    holes = outside.crop((1, 1, icon.width + 1, icon.height + 1)).point(lambda v: 255 if v == 0 else 0)
    fill = Image.new("RGBA", icon.size, (40, 40, 40, 255) if dark_symbol else (255, 255, 255, 255))
    filled = Image.alpha_composite(Image.composite(fill, Image.new("RGBA", icon.size), holes), icon)
    return filled


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "Play", "UserData", "button_icons"))
    a = ap.parse_args()
    base = Image.open(os.path.join(ROOT, "texture_pack", "ScoobyHD", ATLAS)).convert("RGBA")
    boxes = regions(base)
    with tempfile.TemporaryDirectory() as tmp:
        for style, buttons in style_variants().items():
            atlas = Image.new("RGBA", base.size, (0, 0, 0, 0))
            for name, (x0, y0, x1, y1) in boxes.items():
                size = min(x1 - x0, y1 - y0)
                source = style.split("_", 1)[0]
                icon = render_svg(os.path.join(ART, source, buttons[name] + ".svg"), size, tmp, source == "nintendo")
                atlas.alpha_composite(icon, ((x0 + x1 - icon.width) // 2, (y0 + y1 - icon.height) // 2))
            os.makedirs(os.path.join(a.out, style), exist_ok=True)
            atlas.save(os.path.join(a.out, style, ATLAS))
            print("wrote", os.path.join(a.out, style, ATLAS))


if __name__ == "__main__":
    main()
