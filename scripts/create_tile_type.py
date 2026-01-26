#!/usr/bin/env python3
import argparse
import re
import shutil
import sys
from pathlib import Path


def to_camel(name: str) -> str:
    parts = re.split(r"[_\-\s]+", name)
    return "".join(p[:1].upper() + p[1:] for p in parts if p)


def replace_placeholders(text: str, upper: str, camel: str, lower: str) -> str:
    text = text.replace("TEMPLATE", upper)
    text = text.replace("Template", camel)
    text = text.replace("template", lower)
    return text


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Create a new tile type from templates/tile_type_template."
    )
    parser.add_argument(
        "name",
        help="Lowercase type name (letters/numbers/underscore), e.g. weather",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Overwrite existing destination folder",
    )
    args = parser.parse_args()

    raw = args.name.strip()
    if not raw:
        print("Error: name is empty.")
        return 2
    if not re.match(r"^[a-z0-9_]+$", raw):
        print("Error: use lowercase letters, numbers, underscore only (e.g. weather, weather_station).")
        return 2

    lower = raw
    upper = raw.upper()
    camel = to_camel(raw)

    root = Path(__file__).resolve().parents[1]
    template_dir = root / "templates" / "tile_type_template"
    dest_dir = root / "src" / "types" / lower

    if not template_dir.exists():
        print(f"Error: template folder not found: {template_dir}")
        return 2

    if dest_dir.exists():
        if not args.force:
            print(f"Error: destination already exists: {dest_dir}")
            return 2
        shutil.rmtree(dest_dir)

    shutil.copytree(template_dir, dest_dir)

    for path in dest_dir.rglob("*"):
        if path.is_dir():
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except Exception:
            continue
        updated = replace_placeholders(text, upper, camel, lower)
        if updated != text:
            path.write_text(updated, encoding="utf-8")

    print("Created new tile type folder:")
    print(f"  {dest_dir}")
    print("Next steps (manual):")
    print("  1) Add enum in src/tiles/tile_config.h")
    print("  2) Declare renderer in src/tiles/tile_renderer.h")
    print("  3) Register type in src/types/types_registry.cpp")
    print("  4) Wire web admin in src/web/web_admin_scripts.cpp")
    return 0


if __name__ == "__main__":
    sys.exit(main())
