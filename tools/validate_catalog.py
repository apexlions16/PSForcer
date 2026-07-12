#!/usr/bin/env python3
import hashlib
import json
import pathlib
import sys

VALID_KINDS = {"game", "base", "update", "patch", "dlc", "extra", "bonus"}


def fail(message: str) -> None:
    raise SystemExit(f"catalog validation failed: {message}")


def main() -> None:
    if len(sys.argv) != 2:
        fail("usage: validate_catalog.py PATH")
    path = pathlib.Path(sys.argv[1])
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("schemaVersion") != 1:
        fail("schemaVersion must be 1")
    items = data.get("items")
    if not isinstance(items, list):
        fail("items must be an array")
    seen_items: set[str] = set()
    for index, item in enumerate(items):
        item_id = item.get("id")
        if not isinstance(item_id, str) or not item_id:
            fail(f"items[{index}].id is required")
        if item_id in seen_items:
            fail(f"duplicate item id: {item_id}")
        seen_items.add(item_id)
        if not isinstance(item.get("title"), str) or not item["title"]:
            fail(f"{item_id}: title is required")
        packages = item.get("packages")
        if not isinstance(packages, list):
            fail(f"{item_id}: packages must be an array")
        seen_packages: set[str] = set()
        for package in packages:
            package_id = package.get("id")
            if not isinstance(package_id, str) or not package_id:
                fail(f"{item_id}: package id is required")
            if package_id in seen_packages:
                fail(f"{item_id}: duplicate package id {package_id}")
            seen_packages.add(package_id)
            if package.get("kind") not in VALID_KINDS:
                fail(f"{item_id}/{package_id}: invalid kind")
            sha = package.get("sha256", "")
            if sha and (len(sha) != 64 or any(c not in "0123456789abcdefABCDEF" for c in sha)):
                fail(f"{item_id}/{package_id}: sha256 must be 64 hex characters")
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    print(f"valid catalog: {len(items)} items, sha256={digest}")


if __name__ == "__main__":
    main()
