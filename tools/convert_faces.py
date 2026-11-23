"""face/*.png を RGB565 + 8bit アルファの C++ ソースに変換する。

    python tools/convert_faces.py
"""

import re
import sys
from pathlib import Path

LAYER_PATTERN = re.compile(r"^(bg|eyes_\w+|mouth_\w+)$")
VALUES_PER_LINE = 16


def project_dir() -> Path:
    return Path(__file__).resolve().parent.parent


def ensure_pillow() -> None:
    try:
        import PIL  # noqa: F401
    except ImportError:
        sys.exit("Pillow が必要です: pip install pillow")


def to_rgb565(r: int, g: int, b: int) -> int:
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def format_array(ctype: str, name: str, values: list[int], hex_width: int) -> str:
    lines = []
    for i in range(0, len(values), VALUES_PER_LINE):
        chunk = values[i : i + VALUES_PER_LINE]
        lines.append("    " + ", ".join(f"0x{v:0{hex_width}X}" for v in chunk) + ",")
    body = "\n".join(lines)
    return f"const {ctype} {name}[] = {{\n{body}\n}};\n"


def convert_layer(path: Path):
    from PIL import Image

    name = path.stem
    image = Image.open(path).convert("RGBA")
    width, height = image.size

    if name == "bg":
        box = (0, 0, width, height)
        has_alpha = False
    else:
        box = image.getchannel("A").getbbox()
        if box is None:
            raise ValueError(f"{path.name} に不透明なピクセルがありません")
        has_alpha = True

    cropped = image.crop(box)
    pixels = []
    alpha = []
    for r, g, b, a in cropped.getdata():
        pixels.append(to_rgb565(r, g, b))
        alpha.append(a)

    x, y, right, bottom = box
    return {
        "name": name,
        "x": x,
        "y": y,
        "w": right - x,
        "h": bottom - y,
        "pixels": pixels,
        "alpha": alpha if has_alpha else None,
        "canvas": (width, height),
    }


def render_header(layers) -> str:
    width, height = layers[0]["canvas"]
    decls = "\n".join(f"extern const FaceLayer {layer['name']};" for layer in layers)
    return (
        "// 自動生成ファイル: tools/convert_faces.py\n"
        "#pragma once\n\n"
        '#include "../face_layer.h"\n\n'
        "namespace face_assets {\n\n"
        f"constexpr int16_t kWidth = {width};\n"
        f"constexpr int16_t kHeight = {height};\n\n"
        f"{decls}\n\n"
        "}  // namespace face_assets\n"
    )


def render_source(layers) -> str:
    data = []
    defs = []
    for layer in layers:
        name = layer["name"]
        data.append(format_array("uint16_t", f"{name}_pixels", layer["pixels"], 4))
        alpha_ref = "nullptr"
        if layer["alpha"] is not None:
            data.append(format_array("uint8_t", f"{name}_alpha", layer["alpha"], 2))
            alpha_ref = f"{name}_alpha"
        defs.append(
            f"const FaceLayer {name} = {{{layer['x']}, {layer['y']}, "
            f"{layer['w']}, {layer['h']}, {name}_pixels, {alpha_ref}}};"
        )
    return (
        "// 自動生成ファイル: tools/convert_faces.py\n"
        '#include "face_assets.h"\n\n'
        "namespace face_assets {\n\n"
        "namespace {\n\n"
        + "\n".join(data)
        + "\n}  // namespace\n\n"
        + "\n".join(defs)
        + "\n\n}  // namespace face_assets\n"
    )


def find_sources(face_dir: Path) -> list[Path]:
    sources = [p for p in face_dir.glob("*.png") if LAYER_PATTERN.match(p.stem)]
    if not any(p.stem == "bg" for p in sources):
        raise FileNotFoundError(f"{face_dir / 'bg.png'} がありません")
    return sorted(sources, key=lambda p: (p.stem != "bg", p.stem))


def is_up_to_date(inputs: list[Path], outputs: list[Path]) -> bool:
    if not all(p.exists() for p in outputs):
        return False
    newest_input = max(p.stat().st_mtime for p in inputs)
    oldest_output = min(p.stat().st_mtime for p in outputs)
    return oldest_output >= newest_input


def main() -> None:
    root = project_dir()
    face_dir = root / "face"
    out_dir = root / "src" / "generated"
    header = out_dir / "face_assets.h"
    source = out_dir / "face_assets.cpp"

    sources = find_sources(face_dir)
    script = root / "tools" / "convert_faces.py"
    if is_up_to_date(sources + [script], [header, source]):
        return

    ensure_pillow()
    layers = [convert_layer(p) for p in sources]
    canvas = layers[0]["canvas"]
    for layer in layers:
        if layer["canvas"] != canvas:
            raise ValueError(f"{layer['name']}.png のサイズが bg.png と異なります")

    out_dir.mkdir(parents=True, exist_ok=True)
    header.write_text(render_header(layers), encoding="utf-8")
    source.write_text(render_source(layers), encoding="utf-8")
    names = ", ".join(layer["name"] for layer in layers)
    print(f"convert_faces: {len(layers)} layers -> {out_dir} ({names})")


if __name__ == "__main__":
    main()
