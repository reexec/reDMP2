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

## 2. 리프트(Lifting) 결과 (**확인됨**, 단계 B → issue #5 패치 `0007`·`0008` 갱신)

`python scripts/recomp/regenerate.py --funcs-only roms` 및 `python scripts/recomp/regenerate.py roms` (`--trampoline`, `ET_EXEC` `--base` 생략, 패치 `0007` + `0008` 적용):

| 지표 | issue #1 (`0007` 전) | issue #5 (`0007` 후) | issue #5 (`0008` 후) |
|---|---:|---:|---:|
| 코드 스캔 범위 | `0x08804000` + `843,552` B (`210,888` 명령어) | 동일 | 동일 |
| 발견 함수 수 | `7,933` (124 `.c`, 127파일) | `8,007` (126 `.c`, 129파일) | **`8,014`** (126 `.c`, 129파일, `+7`) |
| 내부 진입점 수 | `27,936` | `28,090` | **`28,110`** (`+20`) |
| `.text` 커버리지 | `753,592 / 772,856` B (`97.5%`) | `758,836 / 772,856` B (`98.2%`) | **`759,300 / 772,856` B (`98.2%`, `+464` B)** |
| 고유 임포트 호출 | `208` | `211` | **`211`** |
| 간접 분기 / 점프 테이블 | 미해결 `0`, `355` → `4,560` | 미해결 `0`, `365` → `4,670` | 미해결 `0`, **`365` → `4,670`** |
| 발견 명령어 수 | `188,398` (VFPU `84`, invalid `37`) | `189,709` (VFPU `84`, invalid `37`) | **`189,825`** (VFPU `84`, invalid `37`) |
| 빌드 결과 (MSVC x64 Release) | 성공, 경고 0건 | 성공, 경고 0건 | 성공, **경고 0건** |

* 패치 `0007`로 `user_main` 진입점 `0x0880FD20`(`psp_func_0880FD20`)이 리프트되었고, 패치 `0008`로 `.data` 내 C++ 정적 초기화자(`0x088D0BB8`)가 `lui+addiu`로 조립한 원소 생성자 `0x088832B8`(`psp_func_088832B8`)과 소멸자 `0x08883278`(`psp_func_08883278`)도 정상 리프트됨을 **확인됨**.
* PRX 회귀 확인: `reDMP1`의 PRX `BOOT.BIN`에 같은 도구로 `funcs`를 실행한 결과 함수 수 **`9,024`**(`761,988` B, `92.5%`)로 패치 전과 완전히 동일(**확인됨**).

---

## 3. 정적 리프트 감사 (`tools/lift_audit.py`) (**확인됨**, issue #5 `0008` 갱신)

`python tools/lift_audit.py --generated src/game/dmp2/recomp/generated --base 0x08804000`:

* 감사 본문: `8,014`개 / `126`개 파일 (known thunks `36,124`)
* Data misread as code: **`0`** (`0007` + `0008`로 추가된 81개 함수 중 데이터 오인 없음)
* Mismatches: **`12`** (패치 전과 동일한 12건 — `j`로 `psp_import_*()` 꼬리 호출 직후 `void` 본문 종료)
* Tail-call-only cycles (SCCs): **`284`**개 (트램펄린 `--trampoline` 적용)

---

## 4. 미확정 항목

* `.text` 미도달 `2.5%`(`19,264` 바이트)에 간접 호출로만 진입하는 함수가 있는지 여부는 `ET_EXEC` 로드 지원 후 실제 부팅·플레이 중 디스패치 미스 측정으로 확인한다(**미확정**).
* VFPU `84`개 명령어 중 실제 실행 경로에 걸리는 명령이 있는지 여부도 부팅 후 확인한다(**미확정**).

---

# Executable Analysis: `BOOT.BIN` (`djmax`)

This document records confirmed ELF structure, module metadata, and Phase B lift/audit results for `ULKS-46116` v1.09 `PSP_GAME/SYSDIR/BOOT.BIN`.

## 1. Container and Segments (**confirmed**)

`BOOT.BIN` (844,948 bytes, SHA-1 `1a5c904989d430588da9653acad4b6c5cdab2885`) is a plain ELF32 MIPS little-endian **`ET_EXEC` (`e_type = 2`)** fixed at `0x08804000`, with one `PT_LOAD` (`filesz = 0xCDF20`, `memsz = 0x626500`, `rwx`), entry `0x0880FC4C`, module `djmax` at `0x088C0D34`, `$gp = 0x088D9F10`, 2 exports, and 229 imports across 27 libraries. Unlike DMP1's relocatable PRX (`e_type = 0xFFA0`), `BOOT.BIN` is already linked at `0x08804000`, so `regenerate.py` omits `--base` and verifies `p_vaddr == lift.json base`.

## 2. Lift Results (**confirmed**, updated in issue #5 with patches `0007` & `0008`)

With local patches `0007-exec-scan-code-pointers.patch` (re-enabling `.text` `lui+addiu` pointer discovery on `ET_EXEC`) and `0008-exec-scan-outside-text-bodies.patch` (scanning discovered non-`.text` function bodies in the fixpoint loop), `allegrexrecomp emit --trampoline` (no `--base`) produced **8,014 functions** (+7 over `0007`, `28,110` interior entries, +20, `129` files), covering **759,300 / 772,856 bytes (98.2%)** (+464 B) of `.text`, with `211` distinct reached imports, `0` unresolved indirect jumps (`365` jump tables → `4,670` targets), `84` VFPU traps (`0.04%`), and `37` invalid words (`0.02%`). `0x0880FD20`, `0x088832B8`, and `0x08883278` are all lifted, and reDMP1's PRX `BOOT.BIN` stays unchanged at `9,024` functions. Release links cleanly with zero warnings.

## 3. Static Audit (`tools/lift_audit.py`, **confirmed**)

Audited `8,014` bodies (`36,124` thunks): `0` data bodies misread as code, `284` tail-call SCCs (flat under `--trampoline`), and the same `12` `j` mismatches (tail jumps to `psp_import_*()` at the end of `void` bodies).

## 4. Unresolved

Whether any code in the remaining 2.5% of `.text` is reached dynamically via indirect calls, and whether any of the 84 VFPU trap sites are executed at runtime, remain **unresolved** until `ET_EXEC` loading is added to `repsp`.
