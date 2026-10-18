#!/usr/bin/env python3
"""Validate config/menu.json structure for CI and local checks."""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MENU_JSON = ROOT / "config" / "menu.json"
REGISTRATION_JSON = ROOT / "config" / "registration.json"
ICONS_DIR = ROOT / "config" / "icons"
EXPECTED_DEMO_ICONS = {
    "file-F.svg",
    "directory-D.svg",
    "directoryBackground-DB.svg",
    "drive-DR.svg",
    "fileSystemObject-FS.svg",
}

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

KNOWN_EXTENSION_GATES = {"extensionPass"}
KNOWN_ITEM_GATES = {"jsonFilter", "demo:hideTemp"}
KNOWN_PRESENTATION_GATES = {"presentationPass", "demo:readOnlyDisable"}
KNOWN_EXECUTORS = {"messageBox", "launch", "demo:actionLog"}
KNOWN_CHAINS = {
    "extensionGates": KNOWN_EXTENSION_GATES,
    "itemGates": KNOWN_ITEM_GATES,
    "gates": KNOWN_ITEM_GATES,
    "presentationGates": KNOWN_PRESENTATION_GATES,
    "executors": KNOWN_EXECUTORS,
}

REQUIRED_ITEM_FIELDS = ("id", "label", "verb")
ALLOWED_ACTION_TYPES = {"messageBox", "launch"}
ALLOWED_TARGET_TYPES = {
    "file",
    "directory",
    "directoryBackground",
    "drive",
    "fileSystemObject",
}


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


def check_string_array(value: object, path: str) -> None:
    if not isinstance(value, list):
        fail(f"{path} must be an array of strings")
    for index, entry in enumerate(value):
        if not isinstance(entry, str) or not entry.strip():
            fail(f"{path}[{index}] must be a non-empty string")


def check_optional_string(item: dict, key: str, path: str) -> None:
    if key not in item:
        return
    if not isinstance(item[key], str):
        fail(f"{path}.{key} must be a string")


def check_optional_bool(item: dict, key: str, path: str) -> None:
    if key not in item:
        return
    if not isinstance(item[key], bool):
        fail(f"{path}.{key} must be a boolean")


def check_optional_uint(item: dict, key: str, path: str) -> None:
    if key not in item:
        return
    value = item[key]
    if isinstance(value, bool) or not isinstance(value, int) or value < 0 or value > 0xFFFFFFFF:
        fail(f"{path}.{key} must be an unsigned 32-bit integer")


def check_optional_string_array(item: dict, key: str, path: str) -> None:
    if key not in item:
        return
    check_string_array(item[key], f"{path}.{key}")


def check_chain(item: dict, key: str, path: str) -> None:
    if key not in item:
        return
    check_string_array(item[key], f"{path}.{key}")
    unknown = set(item[key]) - KNOWN_CHAINS[key]
    if unknown:
        fail(f"{path}.{key} contains unknown name(s): {sorted(unknown)}")


def validate_item(item: object, index: int) -> None:
    if not isinstance(item, dict):
        fail(f"menuItems[{index}] must be an object")

    path = f"menuItems[{index}]"
    for field in REQUIRED_ITEM_FIELDS:
        value = item.get(field)
        if not isinstance(value, str) or not value.strip():
            fail(f"{path}.{field} is required")

    for field in ("helpText", "canonicalName", "actionTitle", "actionTemplate", "actionCommand"):
        check_optional_string(item, field, path)

    for field in ("separatorAfter", "filesOnly", "foldersOnly", "actionShowWindow"):
        check_optional_bool(item, field, path)

    for field in ("minSelection", "maxSelection"):
        check_optional_uint(item, field, path)

    for field in ("extensions", "excludeExtensions"):
        check_optional_string_array(item, field, path)

    for key in ITEM_CHAIN_KEYS:
        check_chain(item, key, path)

    action_type = item.get("actionType", "messageBox")
    if not isinstance(action_type, str) or action_type not in ALLOWED_ACTION_TYPES:
        fail(f"{path}.actionType must be one of {sorted(ALLOWED_ACTION_TYPES)}")
    if action_type == "launch":
        command = item.get("actionCommand")
        if not isinstance(command, str) or not command.strip():
            fail(f"{path}.actionCommand is required for launch items")

    icon = item.get("icon")
    if icon is not None:
        if not isinstance(icon, str) or not icon.strip():
            fail(f"{path}.icon must be a non-empty string when present")
        icon_path = ROOT / "config" / icon.replace("/", "\\")
        if not icon_path.is_file():
            fail(f"{path}.icon file not found: {icon}")

    targets = item.get("targets")
    if targets is not None:
        check_string_array(targets, f"{path}.targets")
        unknown_targets = set(targets) - ALLOWED_TARGET_TYPES
        if unknown_targets:
            fail(f"{path}.targets contains unknown target(s): {sorted(unknown_targets)}")


def main() -> None:
    if not MENU_JSON.is_file():
        fail(f"Missing config file: {MENU_JSON}")

    try:
        document = json.loads(MENU_JSON.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        fail(f"Invalid JSON in {MENU_JSON}: {exc}")

    if not isinstance(document, dict):
        fail("menu.json root must be an object")

    if not REGISTRATION_JSON.is_file():
        fail(f"Missing config file: {REGISTRATION_JSON}")

    try:
        registration_document = json.loads(REGISTRATION_JSON.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        fail(f"Invalid JSON in {REGISTRATION_JSON}: {exc}")

    if not isinstance(registration_document, dict):
        fail("registration.json root must be an object")

    if registration_document.get("schemaVersion") != 1:
        fail("registration.json schemaVersion must be 1")

    registrations = registration_document.get("shellRegistrations")
    if not isinstance(registrations, list) or not registrations:
        fail("registration.json shellRegistrations must be a non-empty array")
    check_string_array(registrations, "shellRegistrations")
    unknown_registrations = set(registrations) - ALLOWED_TARGET_TYPES
    if unknown_registrations:
        fail(f"shellRegistrations contains unknown target(s): {sorted(unknown_registrations)}")
    if len(registrations) != len(set(registrations)):
        fail("shellRegistrations must not contain duplicates")

    schema_version = document.get("schemaVersion")
    if schema_version is not None:
        if isinstance(schema_version, bool) or schema_version != 1:
            fail("menu.json schemaVersion must be 1")

    for key in ROOT_CHAIN_KEYS:
        check_chain(document, key, "root")

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
        missing_icons = EXPECTED_DEMO_ICONS - {
            icon.name for icon in ICONS_DIR.glob("*.svg")
        }
        if missing_icons:
            fail(f"Missing demo icon(s): {sorted(missing_icons)}")

    print(f"OK: validated {len(items)} menu item(s) in {MENU_JSON.name}")


if __name__ == "__main__":
    main()
