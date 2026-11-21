# 실행 파일 분석: `BOOT.BIN` (`djmax`)

이 문서는 `ULKS-46116` v1.09 `PSP_GAME/SYSDIR/BOOT.BIN`의 ELF 구조, 모듈 정보, 27개 임포트 라이브러리, 그리고 `allegrexrecomp` 리프트·감사 결과를 기록한다.

---

## 1. 컨테이너 및 세그먼트 구조 (**확인됨**)

`allegrexrecomp info roms/PSP_GAME/SYSDIR/BOOT.BIN`으로 확인한 결과:

| 항목 | 값 |
|---|---|
| 파일 크기 | 844,948 바이트 |
| SHA-1 | `1a5c904989d430588da9653acad4b6c5cdab2885` |
| 형식 | ELF32 MIPS Little-Endian, **`ET_EXEC` (`e_type = 2`)** |
| 진입점 (`e_entry`) | `0x0880FC4C` |
| `PT_LOAD` 수 | 1 (`p_vaddr = 0x08804000`, `p_filesz = 843,552` (`0xCDF20`), `p_memsz = 6,448,384` (`0x626500`), `rwx`) |
| 섹션 수 | 24 |
| `.text` | `0x08804000` + `771,024` 바이트 (`0xBC3D0`) |
| import stubs (`.lib.stub` 코드 영역) | `0x088C03D0` + `1,832` 바이트 |
| 모듈 정보 (`.rodata.sceModuleInfo`) | `0x088C0D34`, 이름 `djmax`, `attribute = 0x00000000`, `$gp = 0x088D9F10` |
| exports | `0x088C0AFC..0x088C0B0C` (2개 함수) |
| imports | `0x088C0B14..0x088C0D30` (27개 라이브러리, 229개 함수) |

### reDMP1(`ULKS-46050`)과의 핵심 차이 (**확인됨**)

* DMP1의 `BOOT.BIN`은 재배치 가능한 PRX(`e_type = 0xFFA0`, 링크 베이스 `0x00000000`, `SHT_PRXRELOC` 포함)여서 `0x08804000`으로 재배치해 리프트·로드했다.
* DMP2의 `BOOT.BIN`은 **`0x08804000`에 고정 링크된 `ET_EXEC` (`e_type = 2`)**이다. 따라서 `allegrexrecomp`에는 `--base`를 넘기지 않고 링크 주소 그대로 리프트한다(`scripts/recomp/regenerate.py`가 `e_type`을 검사해 `ET_EXEC`이면 `--base`를 생략하고 첫 `PT_LOAD` `p_vaddr == lift.json base`를 검증한다).

---

## 2. 리프트(Lifting) 결과 (**확인됨**, 단계 B)

`python scripts/recomp/regenerate.py --funcs-only roms` 및 `python scripts/recomp/regenerate.py roms` (`--trampoline`, `ET_EXEC` `--base` 생략):

| 지표 | 값 |
|---|---|
| 코드 스캔 범위 | `0x08804000` + `843,552` 바이트 (`210,888` 명령어) |
| 시드(seeds) | `2,349` (entry `0x0880FC4C` + exports `2` + data pointer 휴리스틱 스캔 `2,346`) |
| 발견 함수 수 | **`7,933`** (`dmp2_funcs_000..123.c`, 총 127개 생성 파일) |
| 내부 진입점 수 | **`27,936`** |
| `.text` 커버리지 | **`753,592 / 772,856` 바이트 (97.5%)** |
| 고유 임포트 호출 | `208`개 (`229`개 선언 중 도달 208개) |
| 간접 분기 | 미해결 `0`, 점프 테이블 `355`개 → `4,560`개 타깃 해석 |
| 발견 명령어 수 | `188,398` (VFPU `84`개 `0.04%` 트랩 방출, invalid `37`개 `0.02%`) |
| 함수 형태 | `touch vfpu` 32 (`0.4%`), `no jr $ra` 4,278, `computed jumps` 272, `scattered` 196, 최대 함수 `0x088CF73C` (4,220 B) |
| 빌드 결과 (MSVC x64 Debug/Release) | `redmp2_recomp.lib` + `redmp2.exe` 컴파일·링크 성공, **경고 0건** |

---

## 3. 정적 리프트 감사 (`tools/lift_audit.py`) (**확인됨**, 단계 B)

`python tools/lift_audit.py --generated src/game/dmp2/recomp/generated --base 0x08804000`:

* 감사 본문: `7,933`개 / `124`개 파일 (known thunks `35,869`)
* 본문 끝 분류: `jr_ra` 3,637, `other` 1,930, `branch` 1,178, `j` 835, `branch_likely` 167, `jal` 113, `jr_reg` 73
* Data misread as code: **`0`**
* Mismatches: **`12`** (`j` 12건 — 모두 본문 끝에서 `psp_import_*()`를 꼬리 호출한 직후 `void` 함수 닫는 중괄호로 종료하는 형태: `0x08820B4C`, `0x0882695C`, `0x088269A0`, `0x08826BA4`, `0x0882DF6C`, `0x0882E22C`, `0x0882E23C`, `0x0882F8C4`, `0x0882F8CC`, `0x088807B4`, `0x088807D4`, `0x08882514`)
* Tail-call-only cycles (SCCs): `284`개 (트램펄린 `--trampoline` 적용)

---

## 4. 미확정 항목

* `.text` 미도달 `2.5%`(`19,264` 바이트)에 간접 호출로만 진입하는 함수가 있는지 여부는 `ET_EXEC` 로드 지원 후 실제 부팅·플레이 중 디스패치 미스 측정으로 확인한다(**미확정**).
* VFPU `84`개 명령어 중 실제 실행 경로에 걸리는 명령이 있는지 여부도 부팅 후 확인한다(**미확정**).

---

# Executable Analysis: `BOOT.BIN` (`djmax`)

This document records confirmed ELF structure, module metadata, and Phase B lift/audit results for `ULKS-46116` v1.09 `PSP_GAME/SYSDIR/BOOT.BIN`.

## 1. Container and Segments (**confirmed**)

`BOOT.BIN` (844,948 bytes, SHA-1 `1a5c904989d430588da9653acad4b6c5cdab2885`) is a plain ELF32 MIPS little-endian **`ET_EXEC` (`e_type = 2`)** fixed at `0x08804000`, with one `PT_LOAD` (`filesz = 0xCDF20`, `memsz = 0x626500`, `rwx`), entry `0x0880FC4C`, module `djmax` at `0x088C0D34`, `$gp = 0x088D9F10`, 2 exports, and 229 imports across 27 libraries. Unlike DMP1's relocatable PRX (`e_type = 0xFFA0`), `BOOT.BIN` is already linked at `0x08804000`, so `regenerate.py` omits `--base` and verifies `p_vaddr == lift.json base`.

## 2. Lift Results (**confirmed**, Phase B)

`allegrexrecomp emit --trampoline` (no `--base`) produced **7,933 functions** (`27,936` interior entries, `127` files), covering **753,592 / 772,856 bytes (97.5%)** of `.text`, with `208` distinct reached imports, `0` unresolved indirect jumps (`355` jump tables → `4,560` targets), `84` VFPU traps (`0.04%`), and `37` invalid words (`0.02%`). Both Win64 Debug and Release link cleanly with zero warnings.

## 3. Static Audit (`tools/lift_audit.py`, **confirmed**)

Audited `7,933` bodies (`35,869` thunks): `0` data bodies, `284` tail-call SCCs (flat under `--trampoline`), and `12` `j` mismatches — all 12 are tail jumps to `psp_import_*()` immediately before the closing `}` of a `void` body.

## 4. Unresolved

Whether any code in the remaining 2.5% of `.text` is reached dynamically via indirect calls, and whether any of the 84 VFPU trap sites are executed at runtime, remain **unresolved** until `ET_EXEC` loading is added to `repsp`.
