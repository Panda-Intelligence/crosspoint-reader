#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys


DEFAULT_INTERVALS = [
    # Latin coverage for French, Spanish, and mixed Western text.
    "0x0000,0x00FF",
    "0x0100,0x017F",
    "0x0300,0x036F",
    "0x2000,0x206F",
    # CJK and Japanese coverage for Chinese text, Japanese kana, punctuation, and full-width forms.
    "0x2E80,0x2EFF",
    "0x2F00,0x2FDF",
    "0x3000,0x303F",
    "0x3040,0x309F",
    "0x30A0,0x30FF",
    "0x31F0,0x31FF",
    "0x4E00,0x9FFF",
    "0xF900,0xFAFF",
    "0xFE30,0xFE4F",
    "0xFF00,0xFFEF",
]


def run(cmd):
    print(" ".join(cmd))
    subprocess.check_call(cmd)


def add_font_stack_argument(parser, name, help_text):
    parser.add_argument(
        name,
        nargs="+",
        metavar="FONT",
        required=(name == "--font"),
        help=help_text,
    )


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Build Mofei storage-backed multilingual CJK font packs from TTF/OTF files. "
            "Generated files keep the notosans_tc_* names because the firmware loader discovers that storage contract."
        )
    )
    add_font_stack_argument(
        parser,
        "--font",
        "Regular TTF/OTF font stack. Put the preferred CJK font first, followed by optional Latin fallback fonts.",
    )
    add_font_stack_argument(parser, "--font-bold", "Optional bold TTF/OTF font stack")
    add_font_stack_argument(parser, "--font-italic", "Optional italic TTF/OTF font stack")
    add_font_stack_argument(parser, "--font-bolditalic", "Optional bold italic TTF/OTF font stack")
    parser.add_argument("--output-dir", default=".mofei-fontpacks", help="Where to write generated .epf files")
    parser.add_argument(
        "--interval",
        action="append",
        default=DEFAULT_INTERVALS,
        help="Unicode interval as min,max; may be repeated. Defaults cover Chinese, Japanese, French, and Spanish.",
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

    for size in [10, 12, 14, 16, 18]:
        for suffix, font_stack in style_fonts:
            if not font_stack:
                continue
            pack_name = f"notosans_tc_{size}{suffix}"
            out_pack = os.path.join(args.output_dir, f"{pack_name}.epf")
            cmd = [sys.executable, converter, pack_name, str(size), *font_stack, "--2bit", "--compress",
                   "--pack-output", out_pack]
            for interval in args.interval:
                cmd.extend(["--additional-intervals", interval])
            run(cmd)


if __name__ == "__main__":
    main()
