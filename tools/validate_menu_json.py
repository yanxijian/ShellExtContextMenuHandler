#!/usr/bin/env python3
"""Validate config/menu.json structure for CI and local checks."""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MENU_JSON = ROOT / "config" / "menu.json"
ICONS_DIR = ROOT / "config" / "icons"

ROOT_CHAIN_KEYS = (
    "extensionGates",
    "itemGates",
    "presentationGates",
    "executors",
    "gates",
)

ITEM_CHAIN_KEYS = (
    "extensionGates",
    "itemGates",
    "presentationGates",
    "executors",
    "gates",
)

REQUIRED_ITEM_FIELDS = ("id", "label", "verb")
ALLOWED_ACTION_TYPES = {"messageBox", "launch"}


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


def check_string_array(value: object, path: str) -> None:
    if not isinstance(value, list):
        fail(f"{path} must be an array of strings")
    for index, entry in enumerate(value):
        if not isinstance(entry, str) or not entry.strip():
            fail(f"{path}[{index}] must be a non-empty string")


def validate_item(item: object, index: int) -> None:
    if not isinstance(item, dict):
        fail(f"menuItems[{index}] must be an object")

    for field in REQUIRED_ITEM_FIELDS:
        value = item.get(field)
        if not isinstance(value, str) or not value.strip():
            fail(f"menuItems[{index}].{field} is required")

    action_type = item.get("actionType", "messageBox")
    if action_type not in ALLOWED_ACTION_TYPES:
        fail(f"menuItems[{index}].actionType must be one of {sorted(ALLOWED_ACTION_TYPES)}")

    icon = item.get("icon")
    if icon is not None:
        if not isinstance(icon, str) or not icon.strip():
            fail(f"menuItems[{index}].icon must be a non-empty string when present")
        icon_path = ROOT / "config" / icon.replace("/", "\\")
        if not icon_path.is_file():
            fail(f"menuItems[{index}].icon file not found: {icon}")

    for key in ITEM_CHAIN_KEYS:
        if key in item:
            check_string_array(item[key], f"menuItems[{index}].{key}")


def main() -> None:
    if not MENU_JSON.is_file():
        fail(f"Missing config file: {MENU_JSON}")

    try:
        document = json.loads(MENU_JSON.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        fail(f"Invalid JSON in {MENU_JSON}: {exc}")

    if not isinstance(document, dict):
        fail("menu.json root must be an object")

    for key in ROOT_CHAIN_KEYS:
        if key in document:
            check_string_array(document[key], key)

    items = document.get("menuItems")
    if not isinstance(items, list) or not items:
        fail("menuItems must be a non-empty array")

    ids: set[str] = set()
    for index, item in enumerate(items):
        validate_item(item, index)
        item_id = item["id"]
        if item_id in ids:
            fail(f"Duplicate menu item id: {item_id}")
        ids.add(item_id)

    if ICONS_DIR.is_dir():
        svg_count = len(list(ICONS_DIR.glob("*.svg")))
        if svg_count == 0:
            print("WARN: config/icons contains no SVG files")

    print(f"OK: validated {len(items)} menu item(s) in {MENU_JSON.name}")


if __name__ == "__main__":
    main()
