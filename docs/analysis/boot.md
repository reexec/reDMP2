# 첫 부팅 측정 및 정적 임포트 커버리지 (`ULKS-46116`)

이 문서는 `ULKS-46116` v1.09 `BOOT.BIN`의 첫 부팅 측정 결과(issue #1 로드 단계 정지 → issue #3 `repsp v0.0.30` 고정 후 첫 런타임 정지)와 `tools/import_coverage.py`로 정적 계측한 `repsp` HLE 커버리지를 기록한다.

---

## 1. 첫 런타임 부팅 측정 결과 (`repsp v0.0.30`, `81558ef`) (**확인됨**)

```mermaid
sequenceDiagram
    participant App as redmp2
    participant Loader as repsp LoadPrx
    participant Guest as BOOT.BIN (lifted)
    participant HLE as repsp NID registry
    App->>Loader: BOOT.BIN, base 0x08804000
    Loader-->>App: image 0x08804000..0x08E2A500, gp 0x088D9F10, entry 0x0880FC4C
    App->>Guest: module_start thread (prio 32, stack 256 KiB)
    Guest->>HLE: SysMemUserForUser 0x7591C7DB (sceKernelSetCompiledSdkVersion)
    HLE-->>App: stop (b) unimplemented NID, exit code 10
```

### 실행 명령 및 출력 요약

```powershell
.\build\win64-release\Release\redmp2.exe roms --headless --max-vblanks 600 --save-dir <temp>\save --crash-dir <temp>\crash
```

* 식별 성공: `DJ Max Portable 2` (`ULKS46116`, v1.09, `BOOT.BIN`, 844,948 B, SHA-1 `1a5c904989d430588da9653acad4b6c5cdab2885`).
* 이미지 로드 성공: `0x08804000..0x08E2A500` (6,448,384 B), `$gp = 0x088D9F10`, `entry = 0x0880FC4C`, `module_start` 스레드(우선순위 32, 스택 256 KiB, `tid = 4`) 생성 후 게스트 실행 진입.
* 정지 보고 요약:
  * **정지 분류**: `(b) Unimplemented kernel NID`
  * **라이브러리 / NID**: `SysMemUserForUser` / `0x7591C7DB` (`sceKernelSetCompiledSdkVersion`)
  * **종료 코드**: `10`
  * **디스패치 / 프레임**: 디스패치 `0`회, 표시 `0`프레임 (`VBlank = 0`, `GE lists = 0`, `Audio output_calls = 0`)
* **로컬 CTest 상태 (**확인됨**)**: 생성 코드 포함 Release 빌드 기준 **31/32 통과, 1 실패**(`app_lift_check` 통과, `app_smoke_registered_roms`는 위 미구현 NID 정지로 실패). 생성 코드 없는 Win64 Debug/Release 빌드는 **30/30 통과** (로컬 `roms/` 포함 시 31/31).
* **issue #5 후속 (`0008-exec-scan-outside-text-bodies.patch`) + `repsp` #67 작업 트리(`5e69d39`) 측정 (**확인됨**)**:
  * `.data` 내 C++ 정적 초기화자(`0x088D0BB8`)가 조립한 `0x088832B8`·`0x08883278`이 리프트된 뒤, 헤드리스 실행은 C++ 정적 초기화, `sceDisplaySetMode`(2회), `sceGeEdramGetAddr`/`sceGeSetCallback`/`sceGeListEnQueue`(2 리스트, 289 명령, 2 finish), `scePowerRegisterCallback`, `sceUmdActivate`/`sceUmdGetDriveStat`, `sceUtilityLoadAvModule`(3회)까지 모두 통과하고 다음 미구현 NID에서 정지한다:
    * **정지 분류**: `(b) Unimplemented kernel NID`
    * **라이브러리 / NID**: `ThreadManForUser:sceKernelCreateVpl (0x56C039B5)`
    * **종료 코드**: `10` (디스패치 `0`, 표시 `0`프레임, `GE: 2 lists, 289 commands, 2 finishes`)
* **이전 기록 (issue #1, `repsp` `7f856eb`)**: `ET_EXEC` 미지원으로 `LoadPrx failed: Unsupported ELF type (expected PRX e_type 0xFFA0)`(종료 코드 `1`, CTest 30/32)에서 정지했으나, `repsp v0.0.30`([reexec/repsp#65](https://github.com/reexec/repsp/issues/65))의 `ET_EXEC` 로드 지원으로 해소되었다.
* **issue #6 첫 프레임 뒤 멈춤 원인 측정 (`repsp` `i69-poll-timeouts` `7ee04b9` + `reDMP2` `v0.0.3`, **확인됨**)**:
  * `repsp` #69 적용 후 `sound_poll_thread`(`0x0882FAE4`, 우선순위 18)는 600 VBlank 동안 `sceKernelDelayThread` 1,738회, `__sceSasCore` 1,739회, `sceAudioOutputPannedBlocking` 1,739회(언더런 0)로 정상 동작하지만, `user_main`(우선순위 32)은 여전히 `sceKernelSetEventFlag(0x11, 0x2)`만 7,200만 회 반복 호출하며 1프레임 뒤 멈춘다.
  * 환경 변수 진단 `REDMP2_TRACE_NID=0x1FB15A32`([trap.c](../../src/recomp_abi/trap.c))로 실측한 결과, 모든 호출의 반환 주소는 `$ra = 0x0882FB74`(`$sp = 0x09FBF400`, `$a0 = 0x11`, `$a1 = 0x2`, `v0 = 0`)이다.
  * `0x0882FB6C`(`psp_func_0882FB0C`, `sound_poll_thread` 시작 직후이자 `stream_sound_poll_thread` `0x0882F0F8` 생성 직전)이 `jal 0x088312C8`을 호출한다. 원본 MIPS 코드에서 `0x088312C8`은 `0x08972158`에서 이벤트 플래그 핸들 `0x11`을 읽고 `$a1 = 2`를 설정해 `j 0x0882E618`로 꼬리 점프하며, `0x0882E618`은 `j 0x088C0540`(`sceKernelSetEventFlag` 임포트 스텁) 한 번으로 `$ra`(`0x0882FB74`)로 복귀한다. **원본 게임 코드에는 바쁜 대기 반복문 자체가 없다**(설계 가설 H1·H2 모두 기각).
  * **확정된 원인(리프터 코드 생성 결함)**:
    1. `0x0882E618`이 `psp_body_088312C8` 내부에 주소 오름차순으로 `L_088312C8:` **바로 앞**에 배치된다.
    2. `BOOT.BIN`(`ET_EXEC`)에서 임포트 스텁 주소(`0x088C0540` 등)가 `is_import(an, target)`일 뿐 아니라 `an->funcs`에도 들어가 `is_function(an, target)`도 참이다(`dmp2_funcs_121.c:psp_func_088C0540`).
    3. 로컬 패치 `patches/psprecomp/0005-emit-trampoline.patch`에서 `emit_static_call_tail()`은 `is_import`를 먼저 검사해 `return;` 없이 `psp_import_088C0540();`만 출력하지만, 호출부(`emit.c` `in.is_jump` 경로)는 `if (!c->trampoline || !is_function(an, in.target))` 조건으로 `return;`을 생략한다. 두 조건이 겹쳐 `L_0882E618:`의 `psp_import_088C0540();` 뒤에 `return;`이 누락되고, C 제어 흐름이 바로 아래 `L_088312C8:` → `goto L_0882E618;`으로 폴스루(fall-through)하며 리프터가 합성한 무한 루프가 형성된다.
    4. 생성 코드 전체에서 `return;`이 누락된 `j <import>` 지점은 총 13곳이며, 나머지 12곳은 C 함수 끝(`}`) 직전이라 우연히 복귀하지만 `0x0882E618`만 뒤에 레이블(`L_088312C8`)이 이어져 무한 루프가 된다.
  * **수정 후 측정 (`0009-tail-jump-import-return.patch` + `0010-exclude-import-stubs-from-funcs.patch`, **확인됨**)**:
    * `0009`(꼬리 `j <import>` 항상 `return;` 출력)와 `0010`(`trace_function`/`a_discover`에서 `is_import_stub()` 필터링)을 적용해 재생성·빌드한 결과, 헤드리스 600 VBlank 실행에서 첫 프레임 뒤 멈춤이 완전히 해소되었다.
    * **정지 분류 / 종료 코드**: `(e) VBlank limit reached (--max-vblanks)` / `0` (새 미구현 NID 없음, 디스패치 미스 `0`).
    * **표시 프레임**: **`298` 프레임** (`interval mean=33.65 ms, max=83.46 ms`, `sceDisplayWaitVblankStart` 596회, `sceDisplaySetFrameBuf` 300회, `sceCtrlReadBufferPositive` 299회).
    * **`sceKernelSetEventFlag` 호출 수**: `72,004,166`회 → **`300`회**.
    * **부팅 진행**: `stream_sound_poll_thread`(`0x0882F0F8`)와 `soundThread`(`0x0882DF74`)가 정상 생성되고 `disc0:/PSP_GAME/USRDIR/movie/opening.pmf`(`480x272`) 오프닝 영상 재생에 진입해 600 VBlank 동안 `sceMpegAvcDecode` 302회, `sceMpegAtracDecode` 225회를 오류 없이 실행한다.
  * **미확정**: 오프닝 영상 이후(또는 버튼 입력으로 스킵 후) 타이틀 화면·메뉴·플레이 경로에서 다음 미구현 NID나 정지 지점이 어디인지 추후 측정한다.
* 참고: `redmp2` CLI는 `<game_directory>`를 첫 번째 위치 인자(`argv[1]`)로 받는다.

---

## 2. 정적 임포트 커버리지 (`tools/import_coverage.py`) (**확인됨**)

실행 명령: `python -I tools/import_coverage.py roms/PSP_GAME/SYSDIR/BOOT.BIN` (기준 `third_party/repsp` v0.0.30 `81558ef` — HLE NID 목록은 `7f856eb`과 동일: 등록 HLE NID `146`개, 공개 이름표 `216`개).

정적 대조에서 확인된 미구현 NID 88개 중 **실제 런타임에 첫 번째로 호출된 NID**는 `SysMemUserForUser`의 `0x7591C7DB`(`sceKernelSetCompiledSdkVersion`)이다(**확인됨**).

### 라이브러리별 구현/미구현 현황

| 라이브러리 | 전체 | 구현 (`repsp`) | 미구현 |
|---|---:|---:|---:|
| `IoFileMgrForUser` | 24 | 11 | 13 |
| `Kernel_Library` | 2 | 2 | 0 |
| `LoadExecForUser` | 2 | 2 | 0 |
| `ModuleMgrForUser` | 5 | 2 | 3 |
| `StdioForUser` | 3 | 3 | 0 |
| `SysMemUserForUser` | 8 | 3 | 5 |
| `ThreadManForUser` | 33 | 27 | 6 |
| `UtilsForUser` | 7 | 4 | 3 |
| `sceSuspendForUser` | 5 | 1 | 4 |
| `sceGe_user` | 10 | 8 | 2 |
| `sceDisplay` | 6 | 6 | 0 |
| `sceCtrl` | 3 | 3 | 0 |
| `sceUmdUser` | 9 | 6 | 3 |
| `sceAudio` | 9 | 6 | 3 |
| `scePower` | 1 | 1 | 0 |
| `sceUtility` | 17 | 8 | 9 |
| `sceDmac` | 1 | 1 | 0 |
| `sceSasCore` | 21 | 13 | 8 |
| `sceMpeg` | 24 | 24 | 0 |
| `sceImpose` | 2 | 0 | 2 |
| `sceHprm` | 2 | 0 | 2 |
| `sceAtrac3plus` | 11 | 10 | 1 |
| `sceNetAdhoc` | 6 | 0 | 6 |
| `sceNetAdhocctl` | 4 | 0 | 4 |
| `sceNet` | 4 | 0 | 4 |
| `sceNetAdhocMatching` | 9 | 0 | 9 |
| `sceRtc` | 1 | 0 | 1 |
| **합계 (27개 라이브러리)** | **229** | **141 (61.6%)** | **88 (38.4%)** |

*(고유 NID 229개 중 141개 구현, 88개 미구현)*

### 미구현 NID 목록 (88개)

이름이 표기된 항목은 `third_party/repsp/src/runtime/nid_names.cpp` 공개 이름표와 일치한 것이고, `(unknown)`은 해당 이름표에 아직 없는 NID다.

* **`IoFileMgrForUser` (13)**: `0x06A70004` (`sceIoMkdir`), `0x54F5FB11`, `0x55F4717D` (`sceIoChdir`), `0x63632449`, `0x779103A0`, `0xACE946E8` (`sceIoGetstat`), `0xB29DDF9C` (`sceIoDopen`), `0xB8A740F4`, `0xE3EB004C` (`sceIoDread`), `0xEB092469` (`sceIoDclose`), `0xF27A9C51`, `0xFF5940B6` (`sceIoCloseAsync`), `0x1117C65F`
* **`ModuleMgrForUser` (3)**: `0xD8B73127`, `0xF0A26395`, `0x8F2DF740`
* **`SysMemUserForUser` (5)**: `0x13A5ABEF` (`sceKernelPrintf`), `0x7591C7DB`, `0xA291F107` (`sceKernelMaxFreeMemSize`), `0xF77D77CB`, `0xF919F628` (`sceKernelTotalFreeMemSize`)
* **`ThreadManForUser` (6)**: `0xDB738F35`, `0x56C039B5`, `0x89B3D48C`, `0xB736E9FF`, `0xBA6B92E2`, `0xBED27435`
* **`UtilsForUser` (3)**: `0x71EC4271`, `0x91E4F6A7`, `0x27CC57F0`
* **`sceSuspendForUser` (4)**: `0x3AEE7261` (`sceKernelPowerUnlock`), `0xA14F40B2`, `0xA569E425`, `0xEADB1BD7` (`sceKernelPowerLock`)
* **`sceGe_user` (2)**: `0x4C06E472` (`sceGeContinue`), `0xB448EC0D` (`sceGeBreak`)
* **`sceUmdUser` (3)**: `0x46EBB729` (`sceUmdCheckMedium`), `0x87533940`, `0xCBE9F02A`
* **`sceAudio` (3)**: `0x95FD0C2D` (`sceAudioChangeChannelConfig`), `0xB7E1D8E7` (`sceAudioChangeChannelVolume`), `0xE2D56B2D` (`sceAudioOutputPanned`)
* **`sceUtility` (9)**: `0x1579A159`, `0x4DB1E739`, `0x6332AA39`, `0x64D50C56`, `0x91E70E35`, `0xA5DA2406` (`sceUtilityGetSystemParamInt`), `0xC629AF26`, `0xF7D8D092`, `0xF88155F6`
* **`sceSasCore` (8)**: `0x267A6DD2`, `0x2C8E6AB3`, `0x50A14DFC`, `0x5F9529F6`, `0x74AE582A`, `0x787D04D5`, `0x9EC3676A`, `0xB7660A23`
* **`sceImpose` (2)**: `0x36AA6E91`, `0x72189C48`
* **`sceHprm` (2)**: `0x2BCEC83E`, `0x40D2F9F0`
* **`sceAtrac3plus` (1)**: `0x2DD3E298`
* **`sceNetAdhoc` (6)**: `0x6F92741B`, `0x7F27BB5E`, `0xA62C6F57`, `0xABED3790`, `0xDFE53E03`, `0xE1D621D7`
* **`sceNetAdhocctl` (4)**: `0x20B317A0`, `0x6402490B`, `0x9D689E13`, `0xE26F226E`
* **`sceNet` (4)**: `0x0BF0A3AE`, `0x281928A9`, `0x39AF39A6`, `0x89360950`
* **`sceNetAdhocMatching` (9)**: `0x2A2A1E07`, `0x32B156B3`, `0x5E3D4B79`, `0x7945ECDA`, `0x93EF3843`, `0xB58E61B7`, `0xCA5EDA6F`, `0xEA3C6108`, `0xF16EAF4F`
* **`sceRtc` (1)**: `0xE7C27D1B` (`sceRtcGetCurrentClockLocalTime`)

---

## 3. 추정 및 미확정 항목

* **추정**: `sceUtility` 미구현 9건 중에는 펌웨어 AV/네트워크 모듈 로드 함수(`sceUtilityLoadAvModule` / `sceUtilityUnloadAvModule` / `sceUtilityLoadNetModule` / `sceUtilityUnloadNetModule`)가 포함되어 있을 가능성이 높다(디스크에 `USRDIR/modules/*.prx`가 없기 때문).
* **미확정**: `0x7591C7DB`(`sceKernelSetCompiledSdkVersion`) 이후 부팅·타이틀·플레이 경로에서 추가로 호출되는 미구현 NID의 정확한 순서는 `repsp`에 해당 HLE를 추가하며 순차 측정으로 확정한다.

---

# First Boot Measurement and Static Import Coverage (`ULKS-46116`)

## 1. First Runtime Boot Measurement (`repsp v0.0.30`, `81558ef`) (**confirmed**)

With `third_party/repsp` pinned to v0.0.30 (`81558ef`, [reexec/repsp#65](https://github.com/reexec/repsp/issues/65)), `BOOT.BIN` now loads cleanly as a fixed-address `ET_EXEC` at `0x08804000..0x08E2A500` (`$gp = 0x088D9F10`, `entry = 0x0880FC4C`), lifting the earlier issue #1 load-stage stop (`Unsupported ELF type`).

Running `redmp2.exe roms --headless --max-vblanks 600 --save-dir <temp>\save --crash-dir <temp>\crash` enters `module_start` (prio 32, 256 KiB stack) and stops immediately on its first import call:
* **Stop class**: `(b) Unimplemented kernel NID`
* **Library / NID**: `SysMemUserForUser` / `0x7591C7DB` (`sceKernelSetCompiledSdkVersion`)
* **Exit code**: `10`
* **Counters**: `0` dispatches, `0` frames presented (`VBlank = 0`, `GE lists = 0`, `Audio output_calls = 0`).
* **Local CTest (**confirmed**)**: Release with generated code passes **31/32** (`app_lift_check` now passes; `app_smoke_registered_roms` fails on this unimplemented NID and is kept enabled). Win64 Debug/Release without generated code pass **30/30** (31/31 locally when `roms/` is present).
* **Issue #5 follow-up (`0008-exec-scan-outside-text-bodies.patch`) with `repsp` #67 working tree (`5e69d39`, **confirmed**)**: Once `0x088832B8` and `0x08883278` are lifted, the headless run passes C++ static initialization, `sceDisplaySetMode` (2 calls), `sceGe` setup and 2 display lists (289 commands, 2 finishes), `scePowerRegisterCallback`, `sceUmdActivate`/`GetDriveStat`, and `sceUtilityLoadAvModule` (3 calls), stopping at `(b) Unimplemented kernel NID`, `ThreadManForUser:sceKernelCreateVpl (0x56C039B5)`, exit code `10`.
* **Issue #6 stall after first frame — root cause measurement (`repsp` `i69-poll-timeouts` `7ee04b9` + `reDMP2` `v0.0.3`, **confirmed**)**:
  * After `repsp` #69, `sound_poll_thread` (`0x0882FAE4`, prio 18) runs normally (`sceKernelDelayThread` 1,738 calls, `__sceSasCore` 1,739 calls, `sceAudioOutputPannedBlocking` 1,739 calls, 0 underruns across 600 vblanks), yet `user_main` (prio 32) still calls `sceKernelSetEventFlag(0x11, 0x2)` ~72M times and presents only 1 frame.
  * Tracing via env-gated `REDMP2_TRACE_NID=0x1FB15A32` ([trap.c](../../src/recomp_abi/trap.c)) confirms every call has `$ra = 0x0882FB74` (`$sp = 0x09FBF400`, `$a0 = 0x11`, `$a1 = 0x2`, `v0 = 0`).
  * `0x0882FB6C` (in `psp_func_0882FB0C`, right after starting `sound_poll_thread` and right before creating `stream_sound_poll_thread` at `0x0882F0F8`) calls `jal 0x088312C8`. In original MIPS, `0x088312C8` loads event flag `0x11` from `0x08972158`, sets `$a1 = 2`, tail-jumps to `0x0882E618`, which tail-jumps to `0x088C0540` (`sceKernelSetEventFlag`) and returns once to `$ra = 0x0882FB74`. **There is no busy loop in the original guest code** (both hypotheses H1 and H2 are refuted).
  * **Confirmed root cause (lifter codegen bug)**: In `psp_body_088312C8`, `L_0882E618:` is emitted immediately before `L_088312C8:` (ascending address order). Because `BOOT.BIN` import stubs (`0x088C0540`, etc.) are both `is_import` and `is_function` (`an->funcs` contains `psp_func_088C0540`), `patches/psprecomp/0005-emit-trampoline.patch` triggers a mismatch: `emit_static_call_tail()` takes the `is_import` branch (emitting `psp_import_088C0540();` without `return;`), while the `in.is_jump` caller checks `if (!c->trampoline || !is_function(an, in.target))` and omits `return;`. C control flow falls straight through from `L_0882E618:` into `L_088312C8:` → `goto L_0882E618;`, creating a lifter-synthesized infinite loop. Across all generated code, 13 `j <import>` sites omit `return;`; 12 happen to precede `}` while `0x0882E618` is followed by `L_088312C8`.
  * **Post-fix measurement (`0009-tail-jump-import-return.patch` + `0010-exclude-import-stubs-from-funcs.patch`, **confirmed**)**: After regenerating with `0009` (always emitting `return;` after `j <import>`) and `0010` (filtering `is_import_stub()` in `trace_function()`/`a_discover()`), the stall is completely resolved. A 600-vblank headless run exits cleanly at `(e) VBlank limit reached` (exit code `0`, `0` dispatches, no new unimplemented NID), presenting **298 frames** (`mean=33.65 ms`, `sceDisplayWaitVblankStart` 596 calls, `sceDisplaySetFrameBuf` 300 calls, `sceCtrlReadBufferPositive` 299 calls), with `sceKernelSetEventFlag` dropping from `72,004,166` to **`300` calls**. The game starts `stream_sound_poll_thread` (`0x0882F0F8`) and `soundThread` (`0x0882DF74`) and plays `disc0:/PSP_GAME/USRDIR/movie/opening.pmf` (`sceMpegAvcDecode` 302 calls, `sceMpegAtracDecode` 225 calls).
  * **Unresolved**: What happens after `opening.pmf` finishes (or is skipped) toward the title screen, menus, and gameplay will be measured next.

## 2. Static Import Coverage (`tools/import_coverage.py`, **confirmed**)

Running `python -I tools/import_coverage.py roms/PSP_GAME/SYSDIR/BOOT.BIN` against `third_party/repsp` v0.0.30 (`81558ef`, 146 registered HLE NIDs) shows **141 / 229 (61.6%)** imported NIDs already implemented and **88 / 229 (38.4%)** missing across 19 libraries (see table and full NID list above). Of those 88 missing NIDs, the first one actually invoked at runtime is `SysMemUserForUser` `0x7591C7DB` (`sceKernelSetCompiledSdkVersion`) (**confirmed**).

## 3. Inferred and Unresolved

* **Inferred**: The 9 missing `sceUtility` NIDs include firmware AV/Net module loaders (`sceUtilityLoadAvModule` / `sceUtilityLoadNetModule`), consistent with the absence of `USRDIR/modules/*.prx`.
* **Unresolved**: The exact sequence of remaining missing NIDs invoked after `0x7591C7DB` during boot and gameplay will be confirmed incrementally as `repsp` HLE support expands.
