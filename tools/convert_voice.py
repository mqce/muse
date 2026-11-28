"""voice/*.wav を 16 kHz mono int16 の C++ ソースに変換する。

    python tools/convert_voice.py
"""

from __future__ import annotations

import struct
import sys
import wave
from pathlib import Path

CLIPS = ("wake_up", "happy", "thinking", "call", "touch", "giggle", "ehehe")
TARGET_RATE = 16000
VALUES_PER_LINE = 12


def project_dir() -> Path:
    return Path(__file__).resolve().parent.parent


def is_up_to_date(inputs: list[Path], outputs: list[Path]) -> bool:
    if not all(p.exists() for p in outputs):
        return False
    newest_input = max(p.stat().st_mtime for p in inputs)
    oldest_output = min(p.stat().st_mtime for p in outputs)
    return oldest_output >= newest_input


def read_wav(path: Path) -> tuple[int, list[int]]:
    with wave.open(str(path), "rb") as w:
        channels = w.getnchannels()
        width = w.getsampwidth()
        rate = w.getframerate()
        n = w.getnframes()
        raw = w.readframes(n)
    if width != 2:
        raise ValueError(f"{path.name}: 16-bit PCM のみ対応 (got {width * 8}-bit)")
    samples = list(struct.unpack(f"<{len(raw) // 2}h", raw))
    if channels == 2:
        samples = [(samples[i] + samples[i + 1]) // 2 for i in range(0, len(samples), 2)]
    elif channels != 1:
        raise ValueError(f"{path.name}: 1 または 2 ch のみ")
    return rate, samples


def resample_linear(samples: list[int], src_rate: int, dst_rate: int) -> list[int]:
    if src_rate == dst_rate or not samples:
        return samples
    out_len = max(1, int(round(len(samples) * dst_rate / src_rate)))
    if out_len == 1:
        return [samples[0]]
    out = []
    for i in range(out_len):
        pos = i * (len(samples) - 1) / (out_len - 1)
        lo = int(pos)
        hi = min(lo + 1, len(samples) - 1)
        frac = pos - lo
        out.append(int(round(samples[lo] * (1 - frac) + samples[hi] * frac)))
    return out


def format_array(name: str, values: list[int]) -> str:
    lines = []
    for i in range(0, len(values), VALUES_PER_LINE):
        chunk = values[i : i + VALUES_PER_LINE]
        lines.append("    " + ", ".join(str(v) for v in chunk) + ",")
    body = "\n".join(lines)
    return f"const int16_t {name}[] = {{\n{body}\n}};\n"


def main() -> None:
    root = project_dir()
    voice_dir = root / "voice"
    out_dir = root / "src" / "generated"
    header = out_dir / "voice_clips_data.h"
    source = out_dir / "voice_clips_data.cpp"
    script = root / "tools" / "convert_voice.py"

    inputs = [voice_dir / f"{name}.wav" for name in CLIPS]
    for path in inputs:
        if not path.exists():
            sys.exit(f"missing {path}")

    if is_up_to_date(inputs + [script], [header, source]):
        return

    clips = {}
    for name, path in zip(CLIPS, inputs):
        rate, samples = read_wav(path)
        pcm = resample_linear(samples, rate, TARGET_RATE)
        clips[name] = pcm
        print(f"convert_voice: {name} {rate}Hz {len(samples)} -> {TARGET_RATE}Hz {len(pcm)}")

    decls = []
    data = []
    for name in CLIPS:
        pcm = clips[name]
        decls.append(f"extern const int16_t {name}[];")
        decls.append(f"constexpr size_t {name}_frames = {len(pcm)};")
        data.append(format_array(name, pcm))

    out_dir.mkdir(parents=True, exist_ok=True)
    header.write_text(
        "// 自動生成ファイル: tools/convert_voice.py\n"
        "#pragma once\n\n"
        "#include <stddef.h>\n"
        "#include <stdint.h>\n\n"
        "namespace voice_clips {\n\n"
        + "\n".join(decls)
        + "\n\n}  // namespace voice_clips\n",
        encoding="utf-8",
    )
    source.write_text(
        "// 自動生成ファイル: tools/convert_voice.py\n"
        '#include "voice_clips_data.h"\n\n'
        "namespace voice_clips {\n\n"
        + "\n".join(data)
        + "\n}  // namespace voice_clips\n",
        encoding="utf-8",
    )
    print(f"convert_voice: wrote {header.name}, {source.name}")


if __name__ == "__main__":
    main()
