#!/usr/bin/env python3
"""Run allegrexrecomp to emit lifted C code into src/game/dmp2/recomp/generated/."""

import argparse
import json
import struct
import sys
from pathlib import Path

from lifter import run_allegrexrecomp

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
DEFAULT_OUTDIR = REPO_ROOT / "src" / "game" / "dmp2" / "recomp" / "generated"
DEFAULT_LIFT_CONFIG = REPO_ROOT / "src" / "game" / "dmp2" / "recomp" / "lift.json"

ELF_TYPE_EXEC = 2
ELF_TYPE_PRX = 0xFFA0
PT_LOAD = 1


def inspect_elf(path: Path) -> tuple[int | None, int | None]:
    """Return (e_type, first_pt_load_vaddr) for a 32-bit little-endian ELF, or (None, None)."""
    with open(path, "rb") as f:
        header = f.read(52)
        if len(header) < 52 or header[:4] != b"\x7fELF":
            return None, None
        if header[4] != 1 or header[5] != 1:
            return None, None
        e_type = struct.unpack_from("<H", header, 16)[0]
        e_phoff = struct.unpack_from("<I", header, 28)[0]
        e_phentsize = struct.unpack_from("<H", header, 42)[0]
        e_phnum = struct.unpack_from("<H", header, 44)[0]

        first_vaddr = None
        if e_phoff > 0 and e_phentsize >= 32 and e_phnum > 0:
            f.seek(e_phoff)
            for _ in range(e_phnum):
                ph = f.read(e_phentsize)
                if len(ph) < 32:
                    break
                p_type, _p_offset, p_vaddr = struct.unpack_from("<III", ph, 0)
                if p_type == PT_LOAD:
                    first_vaddr = p_vaddr
                    break
        return e_type, first_vaddr


def select_lift_input(game_dir: Path) -> tuple[Path, str]:
    boot_bin = game_dir / "PSP_GAME" / "SYSDIR" / "BOOT.BIN"
    eboot_bin = game_dir / "PSP_GAME" / "SYSDIR" / "EBOOT.BIN"

    if boot_bin.is_file():
        # Check if it's plain ELF
        with open(boot_bin, "rb") as f:
            magic = f.read(4)
        if magic == b"\x7fELF":
            return boot_bin, "BOOT.BIN (plain ELF)"

    if eboot_bin.is_file():
        return eboot_bin, "EBOOT.BIN"

    if boot_bin.is_file():
        return boot_bin, "BOOT.BIN"

    raise FileNotFoundError(f"No executable found in {game_dir / 'PSP_GAME' / 'SYSDIR'}")


def main():
    parser = argparse.ArgumentParser(description="Lift PSP executable to C code")
    parser.add_argument("game_dir", nargs="?", default="roms", help="Path to extracted game directory (default: roms)")
    parser.add_argument("--config", default=str(DEFAULT_LIFT_CONFIG), help="Path to lift.json config")
    parser.add_argument("--outdir", default=str(DEFAULT_OUTDIR), help="Output directory for generated C files")
    parser.add_argument("--prefix", default=None, help="Symbol/file prefix (defaults to lift.json prefix)")
    parser.add_argument("--base", default=None, help="Base address for PRX relocation or ET_EXEC verification (defaults to lift.json base)")
    parser.add_argument("--trampoline", action="store_true", default=None, help="Emit trampoline tail transfers (defaults to lift.json)")
    parser.add_argument("--no-trampoline", action="store_false", dest="trampoline", help="Disable trampoline tail transfers")
    parser.add_argument("--timeout", type=float, default=120.0, help="Timeout in seconds for allegrexrecomp (default: 120)")
    parser.add_argument("--info-only", action="store_true", help="Only run allegrexrecomp info")
    parser.add_argument("--funcs-only", action="store_true", help="Only run allegrexrecomp funcs")
    args = parser.parse_args()

    base = args.base
    prefix = args.prefix
    trampoline = args.trampoline
    config_path = Path(args.config)
    if config_path.is_file():
        try:
            with open(config_path, "r", encoding="utf-8") as f:
                cfg = json.load(f)
                if not base and "base" in cfg:
                    base = cfg["base"]
                if not prefix and "prefix" in cfg:
                    prefix = cfg["prefix"]
                if trampoline is None and "trampoline" in cfg:
                    trampoline = bool(cfg["trampoline"])
        except Exception as e:
            print(f"Warning: could not read {config_path}: {e}", file=sys.stderr)

    if trampoline is None:
        trampoline = False

    if not prefix:
        prefix = "dmp2"

    game_dir = Path(args.game_dir).resolve()
    try:
        input_path, desc = select_lift_input(game_dir)
    except FileNotFoundError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"Selected lift input: {input_path} [{desc}]")

    if args.info_only:
        proc = run_allegrexrecomp(["info", str(input_path)], timeout=args.timeout)
        sys.exit(proc.returncode)

    e_type, first_vaddr = inspect_elf(input_path)
    pass_base: str | None = None
    if e_type == ELF_TYPE_PRX:
        pass_base = base
        print(f"ELF type: PRX (0xFFA0); passing --base {pass_base or '0x00000000'}")
    elif e_type == ELF_TYPE_EXEC:
        if first_vaddr is None:
            print(f"Error: ET_EXEC input {input_path} has no PT_LOAD segment.", file=sys.stderr)
            sys.exit(1)
        if base:
            expected_base = int(base, 0)
            if first_vaddr != expected_base:
                print(
                    f"Error: ET_EXEC first PT_LOAD vaddr 0x{first_vaddr:08X} does not match "
                    f"lift.json base 0x{expected_base:08X}.",
                    file=sys.stderr,
                )
                sys.exit(1)
        print(f"ELF type: ET_EXEC (2) fixed at 0x{first_vaddr:08X}; omitting --base")
    else:
        print(
            f"Error: Unsupported lift input ELF type {hex(e_type) if e_type is not None else 'non-ELF'} "
            f"(expected ET_EXEC 2 or PRX 0xFFA0).",
            file=sys.stderr,
        )
        sys.exit(1)

    if args.funcs_only:
        cmd = ["funcs", str(input_path)]
        if pass_base:
            cmd.extend(["--base", pass_base])
        proc = run_allegrexrecomp(cmd, timeout=args.timeout)
        sys.exit(proc.returncode)

    outdir = Path(args.outdir).resolve()
    outdir.mkdir(parents=True, exist_ok=True)

    print(
        f"Emitting C code to: {outdir} "
        f"(prefix: {prefix}, load_base: {base or '0x00000000'}, "
        f"reloc_base_arg: {pass_base or 'none'}, trampoline: {trampoline})"
    )
    cmd = ["emit", str(input_path), str(outdir), prefix]
    if pass_base:
        cmd.extend(["--base", pass_base])
    if trampoline:
        cmd.append("--trampoline")
    proc = run_allegrexrecomp(cmd, timeout=args.timeout)
    if proc.returncode != 0:
        print(f"allegrexrecomp emit failed with exit code {proc.returncode}", file=sys.stderr)
        sys.exit(proc.returncode)

    print("\nGeneration finished successfully.")


if __name__ == "__main__":
    main()
