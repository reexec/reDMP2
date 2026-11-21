# 작업 로그: `ET_EXEC`에서 코드 조립 함수 포인터 스캔 복구 (issue #5)

* 이슈: [#5](https://github.com/reexec/reDMP2/issues/5)
* 설계: [20261121-i005-exec-pointer-scan.md](../design/20261121-i005-exec-pointer-scan.md)
* 작업 지시: [20261121-i005-exec-pointer-scan.md](../work-orders/20261121-i005-exec-pointer-scan.md)
* 브랜치: `i005-exec-pointer-scan`

---

## 1. 변경 요약

1. **로컬 패치 추가 (`patches/psprecomp/0007-exec-scan-code-pointers.patch`)**
   * `a_analysis`(`analyze.h`)에 `int skip_lui_scan` 필드를 추가했습니다.
   * `main.c`에서 `reloc_buf`를 넘길 때 `skip_lui_scan = 1`, `reloc_buf` 없이 세그먼트 이미지를 넘길 때 `skip_lui_scan = (e->type != 2)`로 설정했습니다.
   * `analyze.c`의 `.text` `lui+addiu` 포인터 스캔 조건을 `if (an->scan_calls && !an->image)`에서 `if (an->scan_calls && !an->skip_lui_scan)`으로 변경했습니다.
   * 결과적으로 재배치 PRX와 미재배치 PRX(base 0)는 기존과 동일하게 스캔을 생략하고, 재배치 표가 없는 고정 주소 `ET_EXEC`(`e_type == 2`)에서는 upstream과 동일하게 `.text` `lui+addiu` 스캔이 실행됩니다.
2. **CMake 및 패치 안내 갱신**
   * `CMakeLists.txt`의 `ALLEGREXRECOMP_PATCHES` 목록 끝에 `0007-exec-scan-code-pointers.patch`를 추가했습니다(`third_party/psprecomp` 서브모듈 무변경).
   * `patches/psprecomp/README.md`의 패치 목록과 upstream 표에 `0007`("#36에 반영 필요, 미결정")을 추가했습니다.
3. **분석 문서 갱신**
   * [docs/analysis/executable.md](../analysis/executable.md): issue #1 대비 새 리프트 수치 및 `lift_audit.py` 결과 반영.
   * [docs/analysis/boot.md](../analysis/boot.md): `0x0880FD20` 디스패치 미스 해소 및 다음 미구현 NID 정지 반영.

---

## 2. 리프트 및 감사 비교 (**확인됨**)

### 2.1. `ULKS-46116` `BOOT.BIN` (`ET_EXEC`) 전후 비교
```powershell
python scripts/recomp/regenerate.py --funcs-only roms
python scripts/recomp/regenerate.py roms
python tools/lift_audit.py --generated src/game/dmp2/recomp/generated --base 0x08804000
```

| 지표 | issue #1 (패치 `0007` 전) | issue #5 (패치 `0007` 후) | 변화 |
|---|---:|---:|---:|
| 발견 함수 수 | `7,933` (124 `.c`, 총 127파일) | **`8,007`** (126 `.c`, 총 129파일) | **`+74`** |
| 내부 진입점 수 | `27,936` | **`28,090`** | **`+154`** |
| `.text` 커버리지 | `753,592 / 772,856` B (`97.5%`) | **`758,836 / 772,856` B (`98.2%`)** | **`+5,244` B (`+0.7%p`)** |
| 고유 임포트 호출 | `208` | **`211`** | **`+3`** |
| 점프 테이블 | `355`개 → `4,560` 타깃 (미해결 `0`) | **`365`개 → `4,670` 타깃** (미해결 `0`) | **`+10` (`+110` 타깃)** |
| 발견 명령어 수 | `188,398` (VFPU `84`, invalid `37`) | **`189,709`** (VFPU `84`, invalid `37`) | **`+1,311`** |
| `lift_audit` Data misread as code | `0` | **`0`** | `0` |
| `lift_audit` Mismatches / SCCs | `12` / `284` | **`12` / `284`** | `0` |
| `0x0880FD20` 리프트 여부 | 미등록 (디스패치 미스) | **`psp_func_0880FD20` 등록 확인** | 해소 |

### 2.2. `reDMP1` PRX 회귀 검증 (읽기 전용, **확인됨**)
```powershell
.\build\win64-release\Release\allegrexrecomp.exe funcs E:\MYWORK\Projects\reexec\reDMP1\roms\PSP_GAME\SYSDIR\BOOT.BIN --base 0x08804000
```
* 패치 전: **`9,024`** 함수 (`761,988` B, `92.5%` of `.text`, `144` imports)
* 패치 후: **`9,024`** 함수 (`761,988` B, `92.5%` of `.text`, `144` imports) — **완전 동일**.

---

## 3. 빌드, CTest 및 헤드리스 부트 검증 (**확인됨**)

*(참고: 아래 실행에 사용한 `third_party/repsp` 작업 트리는 `repsp` #67 브랜치 `i67-boot-path-hle`의 `43ac495`이며, 이 issue의 커밋에는 서브모듈 포인터를 포함하지 않았습니다.)*

### 3.1. 생성 코드 없는 빌드 (CI 조건)
* **Win64 Debug (`-DREDMP2_GENERATED_DIR=<empty>`)**: CTest **31/31 통과** (6.82초; 로컬 `roms/` 식별 테스트 1건 포함, CI 기준 30/30). `redmp2`/`repsp` 타깃 경고 **0건**.
* **Win64 Release (`-DREDMP2_GENERATED_DIR=<empty>`)**: CTest **31/31 통과** (4.19초). `redmp2`/`repsp` 타깃 경고 **0건**.

### 3.2. 생성 코드 포함 Release 빌드, CTest, 헤드리스 부트
```powershell
cmake --preset win64-release
cmake --build build/win64-release --config Release
ctest --test-dir build/win64-release -C Release --output-on-failure
.\build\win64-release\Release\redmp2.exe roms --headless --max-vblanks 600 --save-dir "$env:TEMP\redmp2-i005\save" --crash-dir "$env:TEMP\redmp2-i005\crash"
```
* **빌드 경고**: **0건**.
* **CTest**: **31/32 통과, 1 실패** (`app_lift_check` 통과, `app_smoke_registered_roms`는 아래 미구현 NID 정지로 실패).
* **헤드리스 정지 보고 요약**:
  * `0x0880FD20`(`user_main`) 진입 성공, `SysMemUserForUser:sceKernelAllocPartitionMemory` / `sceKernelGetBlockHeadAddr` 1회씩 호출해 `UserSbrk` (`0x08E2B000..0x08EBD000`, 598,016 B) 할당 성공.
  * **정지 분류**: `(b) Unimplemented kernel NID`
  * **라이브러리 / NID**: `ModuleMgrForUser:sceKernelGetModuleIdByAddress (0xD8B73127)`
  * **종료 코드**: `10` (디스패치 `0`, 표시 `0`프레임)

---

## 4. 후속 (단계 8, 설계 §6): `.text` 밖 발견 함수 본문의 `lui+addiu` 스캔 (`0008`)

* **패치 선택**: `0007`(PR #36 회귀 수정)과 역할을 분리해 별도 패치 `patches/psprecomp/0008-exec-scan-outside-text-bodies.patch`로 추가했습니다. `a_discover` 고정점 루프의 `round > 0` 분기에서 `scan_calls && !skip_lui_scan`일 때 `owner[i] != A_NO_OWNER`이면서 `.text` 바깥에 있는 본문 명령어만 `lui+addiu`/`ori` 쌍으로 스캔해 `.text` 진입점으로 승격합니다(`.data` 선형 스캔 없음).
* **`0007` 대비 리프트·감사 변화 (**확인됨**)**:
  * 함수 수: `8,007` → **`8,014`** (`+7`)
  * 내부 진입점: `28,090` → **`28,110`** (`+20`)
  * `.text` 도달: `758,836` B → **`759,300` B** (`98.2%`, `+464` B)
  * 명령어: `189,709` → **`189,825`** (`+116`), VFPU `84` / invalid `37` 불변
  * 점프 테이블: `365` → **`365`** (`4,670` 타깃, 미해결 `0`)
  * `lift_audit` Data misread as code: **`0`** (Mismatches `12`, SCCs `284` 불변)
  * `0x088832B8` (`psp_func_088832B8`) 및 `0x08883278` (`psp_func_08883278`): **모두 리프트 확인**
  * `reDMP1` PRX 함수 수: **`9,024`** (완전 불변)
* **생성 코드 포함 Release 빌드·CTest·헤드리스 부트 (`repsp` #67 `5e69d39` 사용, **확인됨**)**:
  * 빌드 경고 **0건**, CTest **31/32 통과** (`app_lift_check` 통과, `app_smoke_registered_roms`는 아래 미구현 NID 정지).
  * 헤드리스 실행(`--save-dir`/`--crash-dir` 임시 경로): `0x088832B8` 디스패치 미스를 통과하고 `sceDisplaySetMode`(2회), `sceGe` 2 리스트(289 명령, 2 finish), `scePowerRegisterCallback`, `sceUmdActivate`/`GetDriveStat`, `sceUtilityLoadAvModule`(3회)까지 진행한 뒤 다음 미구현 NID에서 정지:
    * **정지 분류**: `(b) Unimplemented kernel NID`
    * **라이브러리 / NID**: `ThreadManForUser:sceKernelCreateVpl (0x56C039B5)`
    * **종료 코드**: `10` (디스패치 `0`, 표시 `0`프레임)

---

# Work Log: Restore `.text` `lui+addiu` Pointer Scan on `ET_EXEC` (issue #5)

* Issue: [#5](https://github.com/reexec/reDMP2/issues/5)
* Design: [20261121-i005-exec-pointer-scan.md](../design/20261121-i005-exec-pointer-scan.md)
* Work order: [20261121-i005-exec-pointer-scan.md](../work-orders/20261121-i005-exec-pointer-scan.md)
* Branch: `i005-exec-pointer-scan`

---

## 1. Summary of Changes

1. **Local patch (`patches/psprecomp/0007-exec-scan-code-pointers.patch`)**: Added `skip_lui_scan` to `a_analysis` (`analyze.h`), set it in `main.c` for relocated PRX images and non-`ET_EXEC` (`e->type != 2`) images, and changed `analyze.c` from `an->scan_calls && !an->image` to `an->scan_calls && !an->skip_lui_scan`. PRX behaviour is unchanged while fixed-address `ET_EXEC` runs the `.text` `lui+addiu` pointer scan.
2. **CMake & README**: Added `0007-exec-scan-code-pointers.patch` to `ALLEGREXRECOMP_PATCHES` in `CMakeLists.txt` and documented it in `patches/psprecomp/README.md` ("needs to go into #36, undecided"). `third_party/psprecomp` was not edited.
3. **Analysis docs**: Updated [docs/analysis/executable.md](../analysis/executable.md) and [docs/analysis/boot.md](../analysis/boot.md).

## 2. Lift, Audit, and PRX Regression Results (**confirmed**)

* **DMP2 `BOOT.BIN` (`ET_EXEC`)**: functions `7,933` → **`8,007`** (+74), interior entries `27,936` → **`28,090`** (+154), `.text` coverage `97.5%` → **`98.2%`** (`758,836 / 772,856` B, +5,244 B), imports `208` → **`211`** (+3), jump tables `355` → **`365`** (`4,670` targets, +110, 0 unresolved), `lift_audit.py` Data misread as code **`0`**, mismatches `12` / SCCs `284` unchanged. **`psp_func_0880FD20`** is now lifted.
* **reDMP1 `BOOT.BIN` (PRX, read-only)**: **`9,024`** functions before and after (`761,988` B, `92.5%`, `144` imports) — completely unchanged.

## 3. Build, CTest, and Headless Boot (**confirmed**)

*(Note: runs used the `third_party/repsp` working tree on `i67-boot-path-hle` `43ac495`; the submodule pointer is not included in this issue's commits.)*

* **Win64 Debug & Release without generated code**: **31/31 CTest passed** in both (30/30 CI tests + local `roms/` identification test), 0 warnings on `redmp2`/`repsp` targets.
* **Win64 Release with generated code**: 0 warnings, **31/32 CTest passed** (`app_lift_check` passed; `app_smoke_registered_roms` failed on the next unimplemented NID).
* **Headless boot (`--save-dir`/`--crash-dir` outside repo)**: Entered `user_main` (`0x0880FD20`), allocated `UserSbrk` (598,016 B at `0x08E2B000`), and stopped at `(b) Unimplemented kernel NID`, `ModuleMgrForUser:sceKernelGetModuleIdByAddress (0xD8B73127)`, exit code `10` (`0` dispatches, `0` frames).

## 4. Follow-up (Step 8, Design §6): Non-`.text` Discovered Body `lui+addiu` Scan (`0008`)

* **Patch choice**: Added as a separate patch `patches/psprecomp/0008-exec-scan-outside-text-bodies.patch` (keeping `0007` scoped to the PR #36 regression). In `a_discover`'s fixpoint loop (`round > 0`), when `scan_calls && !skip_lui_scan`, scans only `owner[i] != A_NO_OWNER` body instructions outside `.text` for `lui+addiu`/`ori` pairs targeting `.text` (no linear `.data` scan).
* **Delta vs. after-`0007` (**confirmed**)**: functions `8,007` → **`8,014`** (+7), interior entries `28,090` → **`28,110`** (+20), `.text` reached `758,836` → **`759,300` B** (`98.2%`, +464 B), jump tables `365` → `365` (`4,670` targets), `lift_audit` Data misread **`0`**, `psp_func_088832B8` & `psp_func_08883278` both lifted, `reDMP1` PRX unchanged at **`9,024`** functions.
* **Release with generated code (`repsp` #67 `5e69d39`, **confirmed**)**: 0 warnings, **31/32 CTest passed** (`app_lift_check` passed). Headless boot passes C++ static init, `sceDisplaySetMode` (2), `sceGe` (2 lists, 289 commands, 2 finishes), `scePowerRegisterCallback`, `sceUmdActivate`/`GetDriveStat`, and `sceUtilityLoadAvModule` (3), stopping at `(b) Unimplemented kernel NID`, `ThreadManForUser:sceKernelCreateVpl (0x56C039B5)`, exit code `10`.
