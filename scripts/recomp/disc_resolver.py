#!/usr/bin/env python3
"""Shared disc directory resolution for reDMP2 scripts and tools.

Resolution rule for an input path P (matching C++ IdentifyGameDirectory):
1. If P/PSP_GAME/PARAM.SFO exists, P is a disc folder.
2. Otherwise P is treated as a collection folder of discs:
   - Scan immediate subfolders containing PSP_GAME/PARAM.SFO.
   - Empty folders, folders without PARAM.SFO, and unreadable folders are skipped quietly.
   - Select the disc whose DISC_ID is registered in variants.json as the main disc.
   - If zero or more than one registered disc is found, raise DiscResolutionError.
3. Other valid discs found in the collection folder are returned as link-disc candidates.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
DEFAULT_VARIANTS_JSON = REPO_ROOT / "src" / "game" / "dmp2" / "recomp" / "variants.json"


class DiscResolutionError(Exception):
    """Raised when an input path cannot be resolved to a unique main disc."""


def parse_sfo(data: bytes) -> dict[str, str | int]:
    if len(data) < 20 or data[:4] != b"\x00PSF":
        raise ValueError("Invalid SFO magic")

    _, _, key_offset, data_offset, num_entries = struct.unpack_from("<4sIIII", data, 0)
    entries: dict[str, str | int] = {}

    entry_struct = struct.Struct("<HHIII")
    for i in range(num_entries):
        k_off, fmt, d_len, _, d_off = entry_struct.unpack_from(data, 20 + i * entry_struct.size)
        k_start = key_offset + k_off
        k_end = data.find(b"\x00", k_start)
        key = data[k_start:k_end].decode("utf-8", errors="replace")

        v_start = data_offset + d_off
        if fmt == 0x0204:  # UTF-8 string
            val_bytes = data[v_start : v_start + d_len].rstrip(b"\x00")
            entries[key] = val_bytes.decode("utf-8", errors="replace")
        elif fmt == 0x0404:  # 32-bit uint
            entries[key] = struct.unpack_from("<I", data, v_start)[0]
        else:
            entries[key] = data[v_start : v_start + d_len]

    return entries


def normalize_disc_id(disc_id: str) -> str:
    return disc_id.replace("-", "").strip().upper()


def load_registered_disc_ids(variants_path: Path = DEFAULT_VARIANTS_JSON) -> set[str]:
    if not variants_path.is_file():
        return set()
    try:
        with open(variants_path, "r", encoding="utf-8") as f:
            variants = json.load(f)
    except Exception:
        return set()
    ids: set[str] = set()
    if isinstance(variants, list):
        for entry in variants:
            if isinstance(entry, dict) and entry.get("disc_id"):
                ids.add(normalize_disc_id(str(entry["disc_id"])))
    return ids


def resolve_main_disc(
    input_path: Path,
    variants_path: Path = DEFAULT_VARIANTS_JSON,
) -> tuple[Path, list[tuple[str, Path]]]:
    """Resolve input_path to (main_disc_dir, [(disc_id, path), ...] extra_discs)."""
    path = input_path.resolve()
    if not path.is_dir():
        raise DiscResolutionError(f"Directory not found: {path}")

    # Rule 1: Direct disc folder (also supports legacy roms/PSP_GAME layout).
    if (path / "PSP_GAME" / "PARAM.SFO").is_file():
        return path, []

    # Rule 2: Collection folder of discs.
    registered_ids = load_registered_disc_ids(variants_path)
    matched: list[tuple[str, Path]] = []
    extra_discs: list[tuple[str, Path]] = []
    found_ids: list[str] = []

    try:
        entries = sorted(path.iterdir(), key=lambda p: p.name)
    except OSError as e:
        raise DiscResolutionError(f"Failed to scan directory {path}: {e}") from e

    for entry in entries:
        try:
            if not entry.is_dir():
                continue
            sfo_path = entry / "PSP_GAME" / "PARAM.SFO"
            if not sfo_path.is_file():
                continue
            sfo = parse_sfo(sfo_path.read_bytes())
        except Exception:
            continue

        disc_id = str(sfo.get("DISC_ID", "")).strip()
        if not disc_id:
            continue
        found_ids.append(disc_id)

        if normalize_disc_id(disc_id) in registered_ids:
            matched.append((disc_id, entry))
        else:
            extra_discs.append((disc_id, entry))

    if len(matched) == 1:
        return matched[0][1], extra_discs

    if len(matched) > 1:
        desc = ", ".join(f"{did} ({p})" for did, p in matched)
        raise DiscResolutionError(
            f"Multiple registered main discs found in {path}: {desc}; "
            f"expected a single main disc under {path}/<DISC_ID>/PSP_GAME/PARAM.SFO "
            f"or pass a specific disc folder directly"
        )

    found_desc = ", ".join(found_ids) if found_ids else "none"
    raise DiscResolutionError(
        f"No registered main disc found in {path} (found discs: {found_desc}; "
        f"expected {path}/<DISC_ID>/PSP_GAME/PARAM.SFO or {path}/PSP_GAME/PARAM.SFO)"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Resolve main disc directory from a disc or collection folder.")
    parser.add_argument("path", nargs="?", default="roms", help="Disc folder or collection folder (default: roms)")
    parser.add_argument("--variants", default=str(DEFAULT_VARIANTS_JSON), help="Path to variants.json")
    parser.add_argument("--quiet", action="store_true", help="Suppress error messages on stderr")
    args = parser.parse_args()

    try:
        main_dir, _ = resolve_main_disc(Path(args.path), Path(args.variants))
    except DiscResolutionError as e:
        if not args.quiet:
            print(f"Error: {e}", file=sys.stderr)
        return 1

    print(main_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
