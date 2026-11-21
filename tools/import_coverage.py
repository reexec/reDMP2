#!/usr/bin/env python3
"""Static import coverage report: match ELF .lib.stub NIDs against repsp HLE registrations."""

from __future__ import annotations

import argparse
import hashlib
import re
import struct
import sys
from pathlib import Path

PT_LOAD = 1

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_REPSP_DIR = REPO_ROOT / "third_party" / "repsp"


def sha1_nid(name: str) -> int:
    digest = hashlib.sha1(name.encode("ascii")).digest()
    return struct.unpack_from("<I", digest, 0)[0]


def strip_cpp_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//[^\n]*", "", text)
    return text


def vaddr_to_offset(segments: list[tuple[int, int, int]], vaddr: int) -> int | None:
    for p_vaddr, p_offset, p_filesz in segments:
        if p_vaddr <= vaddr < p_vaddr + p_filesz:
            return p_offset + (vaddr - p_vaddr)
    return None


def read_cstr(data: bytes, offset: int) -> str:
    end = data.find(b"\x00", offset)
    if end == -1:
        end = len(data)
    return data[offset:end].decode("utf-8", errors="replace")


def parse_elf_imports(path: Path) -> list[tuple[str, list[int]]]:
    """Parse .lib.stub entries from a 32-bit little-endian PSP ELF/PRX."""
    data = path.read_bytes()
    if len(data) < 52 or data[:4] != b"\x7fELF":
        raise ValueError(f"Not an ELF file: {path}")
    if data[4] != 1 or data[5] != 1:
        raise ValueError(f"Expected 32-bit little-endian ELF: {path}")

    e_phoff, e_shoff = struct.unpack_from("<II", data, 28)
    e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHHHH", data, 42)

    segments: list[tuple[int, int, int]] = []
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type, p_offset, p_vaddr, _p_paddr, p_filesz = struct.unpack_from("<IIIII", data, off)
        if p_type == PT_LOAD:
            segments.append((p_vaddr, p_offset, p_filesz))

    if not segments:
        raise ValueError(f"No PT_LOAD segments in {path}")

    if e_shoff == 0 or e_shnum == 0 or e_shstrndx >= e_shnum:
        raise ValueError(f"Missing section headers in {path}")

    shstr_hdr = e_shoff + e_shstrndx * e_shentsize
    shstr_off = struct.unpack_from("<I", data, shstr_hdr + 16)[0]

    stub_off = None
    stub_size = 0
    for i in range(e_shnum):
        sh = e_shoff + i * e_shentsize
        sh_name, _sh_type, _sh_flags, _sh_addr, s_off, s_size = struct.unpack_from("<IIIIII", data, sh)
        name = read_cstr(data, shstr_off + sh_name)
        if name == ".lib.stub":
            stub_off = s_off
            stub_size = s_size
            break

    if stub_off is None:
        raise ValueError(f"No .lib.stub section found in {path}")

    libs: list[tuple[str, list[int]]] = []
    pos = stub_off
    end = stub_off + stub_size
    while pos + 20 <= end:
        name_ptr, _ver, _flags, len_words, _nvars, nfuncs, nid_ptr, _stub_ptr = struct.unpack_from(
            "<IHHBBHII", data, pos
        )
        entry_bytes = len_words * 4
        if entry_bytes < 20:
            break
        pos += entry_bytes

        name_off = vaddr_to_offset(segments, name_ptr)
        nid_off = vaddr_to_offset(segments, nid_ptr)
        if name_off is None or nid_off is None:
            continue
        lib_name = read_cstr(data, name_off)
        nids: list[int] = []
        for idx in range(nfuncs):
            if nid_off + (idx + 1) * 4 <= len(data):
                nids.append(struct.unpack_from("<I", data, nid_off + idx * 4)[0])
        libs.append((lib_name, nids))

    return libs


def collect_repsp_registered_nids(repsp_dir: Path) -> dict[int, tuple[str, str]]:
    """Parse Register() and RegisterByName() calls in third_party/repsp/src/hle/."""
    hle_dir = repsp_dir / "src" / "hle"
    if not hle_dir.is_dir():
        raise FileNotFoundError(f"repsp HLE directory not found: {hle_dir}")

    reg_explicit = re.compile(
        r'\.Register\s*\(\s*"([^"]+)"\s*,\s*(0x[0-9A-Fa-f]+)[uUlL]*\s*,\s*"([^"]+)"',
        flags=re.MULTILINE,
    )
    reg_by_name = re.compile(
        r'\.RegisterByName\s*\(\s*"([^"]+)"\s*,\s*"([^"]+)"',
        flags=re.MULTILINE,
    )

    registered: dict[int, tuple[str, str]] = {}
    for cpp in sorted(hle_dir.rglob("*.cpp")):
        text = strip_cpp_comments(cpp.read_text(encoding="utf-8", errors="replace"))
        for m in reg_explicit.finditer(text):
            lib, nid_hex, func_name = m.group(1), m.group(2), m.group(3)
            registered[int(nid_hex, 16)] = (lib, func_name)
        for m in reg_by_name.finditer(text):
            lib, func_name = m.group(1), m.group(2)
            registered[sha1_nid(func_name)] = (lib, func_name)

    return registered


def collect_known_nid_names(repsp_dir: Path) -> dict[int, tuple[str, str]]:
    """Parse public function names from third_party/repsp/src/runtime/nid_names.cpp."""
    nid_names_cpp = repsp_dir / "src" / "runtime" / "nid_names.cpp"
    if not nid_names_cpp.is_file():
        return {}

    text = strip_cpp_comments(nid_names_cpp.read_text(encoding="utf-8", errors="replace"))
    entry_re = re.compile(r'\{\s*"([^"]+)"\s*,\s*"([^"]+)"\s*\}')
    known: dict[int, tuple[str, str]] = {}
    for m in entry_re.finditer(text):
        lib, func_name = m.group(1), m.group(2)
        known[sha1_nid(func_name)] = (lib, func_name)
    return known


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare PSP executable .lib.stub imports against repsp HLE NID registrations."
    )
    parser.add_argument(
        "binary",
        nargs="?",
        default="roms/PSP_GAME/SYSDIR/BOOT.BIN",
        help="Path to PSP ELF/PRX binary (default: roms/PSP_GAME/SYSDIR/BOOT.BIN)",
    )
    parser.add_argument(
        "--repsp",
        default=str(DEFAULT_REPSP_DIR),
        help="Path to repsp submodule directory (default: third_party/repsp)",
    )
    args = parser.parse_args()

    bin_path = Path(args.binary)
    repsp_dir = Path(args.repsp)

    libs = parse_elf_imports(bin_path)
    registered = collect_repsp_registered_nids(repsp_dir)
    known_names = collect_known_nid_names(repsp_dir)

    print(f"Binary: {bin_path}")
    print(f"repsp HLE registered NIDs: {len(registered)} (known public names: {len(known_names)})")
    print()
    print(f"{'Library':<24} {'Total':>5} {'Impl':>5} {'Miss':>5}")
    print("-" * 43)

    total_funcs = 0
    total_impl = 0
    total_miss = 0
    distinct_nids: set[int] = set()
    distinct_impl: set[int] = set()
    missing_by_lib: list[tuple[str, list[tuple[int, str]]]] = []

    for lib_name, nids in libs:
        impl = 0
        miss_entries: list[tuple[int, str]] = []
        for nid in nids:
            distinct_nids.add(nid)
            if nid in registered:
                impl += 1
                distinct_impl.add(nid)
            else:
                name = known_names[nid][1] if nid in known_names else "(unknown)"
                miss_entries.append((nid, name))
        miss = len(miss_entries)
        total_funcs += len(nids)
        total_impl += impl
        total_miss += miss
        print(f"{lib_name:<24} {len(nids):>5} {impl:>5} {miss:>5}")
        if miss_entries:
            missing_by_lib.append((lib_name, miss_entries))

    print("-" * 43)
    print(f"{'TOTAL (' + str(len(libs)) + ' libs)':<24} {total_funcs:>5} {total_impl:>5} {total_miss:>5}")
    print(
        f"Distinct NIDs: {len(distinct_nids)} total, "
        f"{len(distinct_impl)} implemented, {len(distinct_nids) - len(distinct_impl)} missing"
    )
    print()
    print("Missing NIDs by library:")
    for lib_name, miss_entries in missing_by_lib:
        print(f"  [{lib_name}] ({len(miss_entries)} missing)")
        for nid, name in miss_entries:
            print(f"    0x{nid:08X}  {name}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
