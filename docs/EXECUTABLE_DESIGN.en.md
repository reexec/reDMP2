# Original Executable Design Notes (DJ Max Portable 2, `ULKS-46116` v1.09)

This document accumulates confirmed structural facts about the original executable. Detailed evidence lives in the topic documents under `docs/analysis/`.

---

## 1. Executable Identity and Memory Layout (**confirmed**)

* Target executable: `PSP_GAME/SYSDIR/BOOT.BIN` (844,948 bytes, SHA-1 `1a5c904989d430588da9653acad4b6c5cdab2885`)
* Format: ELF32 MIPS Little-Endian, **`ET_EXEC` (`e_type = 2`)** — fixed-address link (no PRX relocations)
* Single `PT_LOAD` segment:
  * `p_vaddr = 0x08804000`
  * `p_filesz = 0xCDF20` (843,552 bytes)
  * `p_memsz = 0x626500` (6,448,384 bytes, including ~5.34 MiB BSS)
* Module info (`.rodata.sceModuleInfo` @ `0x088C0D34`):
  * Name: `djmax`
  * Entry point (`e_entry`): `0x0880FC4C`
  * `$gp`: `0x088D9F10`
  * Exports: `0x088C0AFC..0x088C0B0C` (2 functions)
  * Imports: `0x088C0B14..0x088C0D30` (27 libraries, 229 NIDs)

---

## 2. Code Footprint and Static Recompilation (**confirmed**)

* `.text`: `0x08804000` + `0xBC3D0` (771,024 bytes), import stubs `0x088C03D0` + 1,832 bytes.
* `allegrexrecomp emit --trampoline` (omitting `--base` for fixed `ET_EXEC`):
  * `7,933` functions, `27,936` interior entries, `97.5%` `.text` coverage (`753,592 / 772,856` bytes).
  * `355` jump tables resolved to `4,560` targets, `0` unresolved computed jumps.
  * `84` VFPU instructions (`0.04%`), `37` invalid words (`0.02%`).

---

## 3. Firmware Interfaces and Assets (**confirmed** / **inferred**)

* **Confirmed**: Imports 229 distinct NIDs across 27 libraries. Against `repsp` v0.0.30 (`81558ef`), 141 (61.6%) are already implemented and 88 (38.4%) are missing. At runtime, the first unimplemented NID invoked after `module_start` (`0x0880FC4C`) is `SysMemUserForUser` `0x7591C7DB` (`sceKernelSetCompiledSdkVersion`) (see [docs/analysis/boot.md](analysis/boot.md)).
* **Confirmed**: Unlike DMP1, there are no `USRDIR/modules/*.prx` files on disc, and DMP2 adds imports from `sceNet`/`sceNetAdhoc*` (23 NIDs), `sceHprm` (2 NIDs), `sceImpose` (2 NIDs), and `sceRtc` (1 NID).
* **Confirmed**: Disc assets consist of `USRDIR/all.mp`..`all7.mp` (7 archives, ~1.61 GB total) and `USRDIR/movie/*.pmf` (6 movies).
* **Unresolved**: Runtime thread topology, user partition usage, `all*.mp` container layout, and gameplay memory structures (to be analyzed after boot).
