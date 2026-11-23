# 작업 로그: 디스크 ID별 `roms/` 구조 (issue #21)

* 이슈: [#21](https://github.com/reexec/reDMP2/issues/21)
* 설계: [20261124-i021-roms-per-disc.md](../design/20261124-i021-roms-per-disc.md)
* 브랜치: `i021-roms-per-disc`

---

## 1. 단위별 변경 요약

### 앱 및 프로파일 디스크 해석 (`d9f5926`)
* `include/redmp2/dmp2_profile.h`, `src/game/dmp2/dmp2_profile.cpp`:
  * `IdentifyResult`에 `resolved_dir`과 `extra_discs`(`ExtraDisc{disc_id, path}`)를, `IdentifyStatus`에 `AmbiguousDiscDirectory = 6`을 추가했다.
  * `IdentifyGameDirectory(P, variants)`가 설계 §3 규칙대로 동작하도록 확장했다:
    1. `P/PSP_GAME/PARAM.SFO`가 있으면 `P`를 단일 디스크 폴더로 식별한다(예전 `roms/PSP_GAME` 및 직접 `roms/ULKS46116` 모두 지원).
    2. 없으면 `P`를 디스크 모음 폴더로 보고 바로 아래 하위 폴더 중 `PSP_GAME/PARAM.SFO`가 있는 것을 훑어 `variants.json`에 등록된 주 디스크(DMP2)를 고른다. 빈 폴더·`PARAM.SFO` 없는 폴더·읽을 수 없는 폴더는 조용히 건너뛴다.
    3. 등록된 주 디스크가 없거나 둘 이상이면 찾은 디스크 ID 목록과 기대 위치를 담은 오류 메시지로 종료한다.
    4. 주 디스크 외에 찾은 유효 디스크(링크 디스크 후보)는 `extra_discs`에 ID와 경로만 기록한다.
* `src/app/main.cpp`:
  * `FindDefaultVariantsJson`에 `game_dir/../variants.json`과 `game_dir/../../src/game/dmp2/recomp/variants.json`을 추가해 `redmp2.exe roms`와 `redmp2.exe roms/ULKS46116` 모두 기본 `variants.json`을 찾도록 했다.
  * 식별 성공 후 `result.resolved_dir`을 `RunLiftCheck`와 `RunBoot`에 전달하고, `result.extra_discs`가 있으면 항목당 `[redmp2] Link disc candidate: <DISC_ID> (<path>)` 한 줄 로그를 출력한다.

### 공용 Python 해석기, 스크립트·도구 및 CMake (`f390426`)
* `scripts/recomp/disc_resolver.py`: 앱과 동일한 3단계 규칙을 구현한 공용 모듈(`parse_sfo`, `normalize_disc_id`, `load_registered_disc_ids`, `resolve_main_disc`, CLI 진입점)을 추가했다.
* `scripts/recomp/regenerate.py`, `scripts/recomp/register_disc.py`: `resolve_main_disc`를 사용해 `roms`(디스크 모음 폴더)와 `roms/ULKS46116`(개별 디스크 폴더) 모두 지원하도록 수정했다.
* `tools/import_coverage.py`: 기본 인자를 `roms`로 바꾸고 ELF 파일 경로·개별 디스크 폴더·디스크 모음 폴더를 모두 받도록 수정했다.
* `CMakeLists.txt`: `scripts/recomp/disc_resolver.py --quiet`으로 `REDMP2_ROMS_DISC_DIR`을 해석해 `app_smoke_registered_roms`, `reloc_check*` / `lift_invariance`, `app_lift_check`가 새 `roms/ULKS46116/` 구조에서 동작하도록 갱신했다(DMP2 덤프가 없으면 기존처럼 건너뜀).

### 문서 및 빌드 안내 (`84c1c3a`)
* `README.md`, `ARCHITECTURE.md`, `docs/guides/win64-setup-and-build.md`, `docs/guides/running-and-tracing.md`, `scripts/build_win64.bat`: 디스크 ID별 `roms/` 구조(`roms/ULKS46116/` 필수, `roms/ULKS46050/` 선택)와 `roms` / `roms/ULKS46116` 실행 예를 반영했다. 지난 설계·작업 로그·릴리스 노트는 수정하지 않았다.

---

## 2. 검증 결과 (**확인됨**)

### 2.1. 스크립트 및 정적 도구
* `python scripts/recomp/register_disc.py` (`roms`) 및 `register_disc.py roms/ULKS46116`: 모두 `ULKS46116 v1.09` (`BOOT.BIN`, `844948` bytes) 정상 식별.
* `python scripts/recomp/regenerate.py` (`roms`): **함수 7,982, 내부 진입점 28,109**, `0 of 128 files changed` (기존과 동일).
* `python tools/import_coverage.py` (`roms`): **163/229** (`TOTAL (27 libs) 229 / 163 / 66`, 기존과 동일).
* `python tools/lift_audit.py --generated src/game/dmp2/recomp/generated --image <tmp>/boot_etexec.bin --base 0x0`: `Audited 7982 bodies across 125 files`, **`Mismatches found: 0`**, 종료 코드 `0`.

### 2.2. 빌드 및 CTest
* **Win64 Release (생성 코드 포함 + FFmpeg, `build\win64-release-ffmpeg`)**: 프로젝트 소스 컴파일러 경고 **0건**, CTest **34/34 통과** (`app_smoke_registered_roms`, `app_lift_check` 포함).
* **Win64 Debug (생성 코드 미포함, `REDMP2_GENERATED_DIR=<empty>`)**: 프로젝트 소스 컴파일러 경고 **0건**, CTest **9/9 통과**.
* **Win64 Release (생성 코드 미포함, `REDMP2_GENERATED_DIR=<empty>`)**: 프로젝트 소스 컴파일러 경고 **0건**, CTest **9/9 통과**.
* **Linux x64**: CI에서 검증.

### 2.3. 실행 및 실패·후보 케이스 (`--headless --max-vblanks 20`, `--save-dir`/`--crash-dir` 저장소 밖)
| 케이스 | 인자 | 결과 |
|---|---|---|
| 디스크 모음 폴더 (`ULKS46116` + `ULKS46050`) | `redmp2.exe roms` | DMP2 식별 + `[redmp2] Link disc candidate: ULKS46050 (roms\ULKS46050)` 로그 출력, 정상 부팅 (`Exit code: 0`) |
| 개별 디스크 폴더 (`--variants` 생략) | `redmp2.exe roms/ULKS46116` | 기본 `variants.json` 탐색 및 DMP2 식별, 정상 부팅 (`Exit code: 0`) |
| 레거시 구조 임시 폴더 (`<tmp>/legacy/PSP_GAME`) | `redmp2.exe <tmp>/legacy` | DMP2 식별, 정상 부팅 (`Exit code: 0`) |
| 주 디스크 없는 모음 폴더 (빈 폴더 + `TEST00000`) | `redmp2.exe <tmp>/no_main` | `Identification failed: No registered main disc found in ... (found discs: TEST00000; expected ...)` 오류 종료 (`Exit code: 2`) |
| 빈 `ULKS46050/`가 있는 모음 폴더 | `redmp2.exe <tmp>/empty_link` | 빈 폴더 조용히 건너뜀, DMP2 식별 및 정상 부팅 (`Exit code: 0`) |

---

# Work Log: `roms/` per Disc ID (issue #21)

* Issue: [#21](https://github.com/reexec/reDMP2/issues/21)
* Design: [20261124-i021-roms-per-disc.md](../design/20261124-i021-roms-per-disc.md)
* Branch: `i021-roms-per-disc`

---

## 1. Summary of Changes by Unit

### App & Profile Disc Resolution (`d9f5926`)
* `include/redmp2/dmp2_profile.h`, `src/game/dmp2/dmp2_profile.cpp`:
  * Added `resolved_dir` and `extra_discs` (`ExtraDisc{disc_id, path}`) to `IdentifyResult`, and `AmbiguousDiscDirectory = 6` to `IdentifyStatus`.
  * Extended `IdentifyGameDirectory(P, variants)` to follow design §3:
    1. If `P/PSP_GAME/PARAM.SFO` exists, treat `P` as a single disc directory (supports both legacy `roms/PSP_GAME` and direct `roms/ULKS46116`).
    2. Otherwise scan immediate subfolders of `P` containing `PSP_GAME/PARAM.SFO` and pick the registered main disc (DMP2) from `variants.json`, quietly skipping empty, non-disc, and unreadable folders.
    3. Exit with a clear error listing found disc IDs and expected locations if zero or more than one registered disc is found.
    4. Record other valid discs (link disc candidates) by ID and path in `extra_discs`.
* `src/app/main.cpp`:
  * Extended `FindDefaultVariantsJson` with `game_dir/../variants.json` and `game_dir/../../src/game/dmp2/recomp/variants.json` so both `redmp2.exe roms` and `redmp2.exe roms/ULKS46116` find `variants.json` automatically.
  * Passed `result.resolved_dir` into `RunLiftCheck` and `RunBoot`, and logged one `[redmp2] Link disc candidate: <DISC_ID> (<path>)` line per extra disc.

### Shared Python Resolver, Scripts/Tools, and CMake (`f390426`)
* `scripts/recomp/disc_resolver.py`: Added shared module (`parse_sfo`, `normalize_disc_id`, `load_registered_disc_ids`, `resolve_main_disc`, CLI entry point) implementing the same 3-step rule as the C++ app.
* `scripts/recomp/regenerate.py`, `scripts/recomp/register_disc.py`: Switched to `resolve_main_disc` so both `roms` and `roms/ULKS46116` work.
* `tools/import_coverage.py`: Changed default argument to `roms` and accepted an ELF file, disc folder, or collection folder.
* `CMakeLists.txt`: Resolved `REDMP2_ROMS_DISC_DIR` via `scripts/recomp/disc_resolver.py --quiet` so `app_smoke_registered_roms`, `reloc_check*` / `lift_invariance`, and `app_lift_check` run under the per-disc `roms/` layout and skip cleanly when DMP2 is absent.

### Documentation and Build Hint (`84c1c3a`)
* `README.md`, `ARCHITECTURE.md`, `docs/guides/win64-setup-and-build.md`, `docs/guides/running-and-tracing.md`, `scripts/build_win64.bat`: Documented the per-disc `roms/` layout (`roms/ULKS46116/` required, `roms/ULKS46050/` optional) and run examples. Past designs, work logs, and release notes were left untouched.

---

## 2. Verification Results (**confirmed**)

### 2.1. Scripts and Static Tools
* `python scripts/recomp/register_disc.py` (`roms`) and `register_disc.py roms/ULKS46116`: Both identified `ULKS46116 v1.09` (`BOOT.BIN`, `844948` bytes).
* `python scripts/recomp/regenerate.py` (`roms`): **7,982 functions, 28,109 interior entries**, `0 of 128 files changed` (matches baseline).
* `python tools/import_coverage.py` (`roms`): **163/229** (`TOTAL (27 libs) 229 / 163 / 66`, matches baseline).
* `python tools/lift_audit.py --generated src/game/dmp2/recomp/generated --image <tmp>/boot_etexec.bin --base 0x0`: `Audited 7982 bodies across 125 files`, **`Mismatches found: 0`**, exit code `0`.

### 2.2. Builds and CTest
* **Win64 Release (with generated code + FFmpeg, `build\win64-release-ffmpeg`)**: 0 project compiler warnings, CTest **34/34 passed** (including `app_smoke_registered_roms` and `app_lift_check`).
* **Win64 Debug (without generated code, `REDMP2_GENERATED_DIR=<empty>`)**: 0 project compiler warnings, CTest **9/9 passed**.
* **Win64 Release (without generated code, `REDMP2_GENERATED_DIR=<empty>`)**: 0 project compiler warnings, CTest **9/9 passed**.
* **Linux x64**: Verified in CI.

### 2.3. Runtime, Failure, and Candidate Cases (`--headless --max-vblanks 20`, `--save-dir`/`--crash-dir` outside repo)
| Case | Invocation | Result |
|---|---|---|
| Collection folder (`ULKS46116` + `ULKS46050`) | `redmp2.exe roms` | Identified DMP2, logged `[redmp2] Link disc candidate: ULKS46050 (roms\ULKS46050)`, booted cleanly (`Exit code: 0`) |
| Direct disc folder (without `--variants`) | `redmp2.exe roms/ULKS46116` | Found default `variants.json`, identified DMP2, booted cleanly (`Exit code: 0`) |
| Legacy layout temp folder (`<tmp>/legacy/PSP_GAME`) | `redmp2.exe <tmp>/legacy` | Identified DMP2, booted cleanly (`Exit code: 0`) |
| Collection folder without main disc (empty + `TEST00000`) | `redmp2.exe <tmp>/no_main` | Failed clearly with `No registered main disc found in ... (found discs: TEST00000; expected ...)` (`Exit code: 2`) |
| Collection folder with empty `ULKS46050/` | `redmp2.exe <tmp>/empty_link` | Quietly skipped empty folder, identified DMP2, booted cleanly (`Exit code: 0`) |
