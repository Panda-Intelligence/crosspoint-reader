#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys


def run(cmd):
    print(" ".join(cmd))
    subprocess.check_call(cmd)


def main():
    parser = argparse.ArgumentParser(description="Build Traditional Chinese font packs for storage-backed reading.")
    parser.add_argument("--font", required=True, help="Path to the regular Traditional Chinese font file")
    parser.add_argument("--font-bold", help="Optional path to the bold Traditional Chinese font file")
    parser.add_argument("--font-italic", help="Optional path to the italic Traditional Chinese font file")
    parser.add_argument("--font-bolditalic", help="Optional path to the bold italic Traditional Chinese font file")
    parser.add_argument("--output-dir", default=".mofei-fontpacks", help="Where to write generated .epf files")
    parser.add_argument(
        "--interval",
        action="append",
        default=[
            "0x4E00,0x9FFF",
            "0x3000,0x303F",
            "0xFF00,0xFFEF",
            "0x2000,0x206F",
            "0x0000,0x00FF",
        ],
        help="Additional Unicode interval as min,max; may be repeated",
    )
    args = parser.parse_args()

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    converter = os.path.join(root, "lib", "EpdFont", "scripts", "fontconvert.py")
    os.makedirs(args.output_dir, exist_ok=True)

    style_fonts = [
        ("", args.font),
        ("_bold", args.font_bold),
        ("_italic", args.font_italic),
        ("_bolditalic", args.font_bolditalic),
    ]

    for size in [12, 14, 16, 18]:
        for suffix, font_path in style_fonts:
            if not font_path:
                continue
            out_pack = os.path.join(args.output_dir, f"notosans_tc_{size}{suffix}.epf")
            cmd = [sys.executable, converter, f"notosans_tc_{size}{suffix}", str(size), font_path, "--2bit",
                   "--compress", "--pack-output", out_pack]
            for interval in args.interval:
                cmd.extend(["--additional-intervals", interval])
            run(cmd)


if __name__ == "__main__":
    main()
