# 작업 로그: 타이틀 화면까지 — 리프트 패치 `0011` (issue #9)

* 이슈: [#9](https://github.com/reexec/reDMP2/issues/9) (연계 HLE: [reexec/repsp#71](https://github.com/reexec/repsp/issues/71))
* 설계: [20261121-i009-title-screen.md](../design/20261121-i009-title-screen.md)
* 브랜치: `i009-title-screen`

---

## 1. 패치 `0011-lui-pair-reg-tracking.patch` 구현

* **원인**: `repsp` #71 1차 반복(`sceIoCloseAsync`) 통과 후 `0x08817218`에서 디스패치 미스가 발생했습니다. `0x08817218`(할당 콜백)과 `0x08817228`(해제 콜백)은 세 곳(`0x088173A4`, `0x08817648`, `0x088179A8`)에서 `lui` 뒤 `k=5`, `k=6`(지연 슬롯) 위치의 `addiu`로 조립되는데, 기존 고정 4명령 탐색 창(`k = 1..4`)을 벗어나 누락되었습니다.
* **수정** ([0011-lui-pair-reg-tracking.patch](../../patches/psprecomp/0011-lui-pair-reg-tracking.patch)):
  * `tools/allegrexrecomp/analyze.c`에 `insn_writes_gpr()`와 공용 `scan_lui_pairs()`를 추가해 `.text` 스캔과 `0008`의 비-`.text` 본문 스캔이 모두 같은 레지스터 추적을 사용하도록 했습니다(`skip_lui_scan == 0`인 `ET_EXEC` 전용, PRX 경로 불변).
  * `lui rX, hi` 이후 최대 16명령까지 순회하며 `rs == rX`인 모든 `addiu`/`ori` 후보(`.text` 내, 비스텁, 유효 명령)를 수집하고, `rX` 덮어쓰기·분기/점프 지연 슬롯 이후·16명령 도달 시 종료합니다.
  * `CMakeLists.txt`, `patches/psprecomp/README.md`, `docs/analysis/executable.md`를 갱신했습니다.

---

## 2. 재생성 및 감사 결과 (**확인됨**, v0.0.4 대비)

| 지표 | v0.0.4 (`0009`+`0010`) | issue #9 (`0011` 후) | 변화 |
|---|---:|---:|---:|
| 발견 함수 수 (`BOOT.BIN`) | `7,978` | **`7,982`** | **`+4`** (`0x08817218`, `0x08817228` 포함) |
| 내부 진입점 수 | `28,109` | **`28,109`** | `0` |
| `.text` 커버리지 | `759,000 / 772,856` B (`98.2%`) | **`759,072 / 772,856` B (`98.2%`)** | **`+72` B** (`+18` 명령어) |
| `lift_audit` 데이터 오독 / 불일치 | `0` / `0` | **`0` / `0`** | 동일 (`284` SCCs) |
| `reDMP1` PRX `funcs` (읽기 전용) | `9,004` (`761,828` B) | **`9,004`** (`761,828` B) | **불변** |

* `dmp2_register.c`에 `psp_register(0x08817218u, psp_func_08817218)`과 `psp_register(0x08817228u, psp_func_08817228)` 등록을 확인했습니다.
* Win64 Release 빌드 경고 **0건**, CTest **32/32 통과**.

---

# Work Log: Up to the Title Screen — Lift Patch `0011` (issue #9)

* Issue: [#9](https://github.com/reexec/reDMP2/issues/9) (paired HLE: [reexec/repsp#71](https://github.com/reexec/repsp/issues/71))
* Design: [20261121-i009-title-screen.md](../design/20261121-i009-title-screen.md)
* Branch: `i009-title-screen`

---

## 1. Patch `0011-lui-pair-reg-tracking.patch`

* **Cause**: After `repsp` #71 iteration 1 (`sceIoCloseAsync`), boot hit a dispatch miss at `0x08817218`. Both `0x08817218` (alloc callback) and `0x08817228` (free callback) are built at three sites (`0x088173A4`, `0x08817648`, `0x088179A8`) with `addiu` at `k=5` and `k=6` (delay slot) after `lui`, outside the fixed 4-instruction window.
* **Fix** ([0011-lui-pair-reg-tracking.patch](../../patches/psprecomp/0011-lui-pair-reg-tracking.patch)): Added `insn_writes_gpr()` and a shared `scan_lui_pairs()` helper in `tools/allegrexrecomp/analyze.c` used by both the `.text` scan and patch `0008`'s non-`.text` body scan (`skip_lui_scan == 0` / `ET_EXEC` only; PRX unchanged). Tracks `rX` up to 16 instructions, emitting every `addiu`/`ori` with `rs == rX` until `rX` is written or after a control transfer's delay slot. Updated `CMakeLists.txt`, `patches/psprecomp/README.md`, and `docs/analysis/executable.md`.

---

## 2. Regeneration & Audit Results (**confirmed**, vs. v0.0.4)

* `BOOT.BIN`: `7,982` functions (`+4`, `0x08817218` and `0x08817228` both registered), `28,109` interior entries (`+0`), `759,072 / 772,856` B (`98.2%`, `+72` B / `+18` instructions).
* `tools/lift_audit.py`: `7,982` bodies across `125` files, `Data misread as code: 0`, `Mismatches found: 0`, `284` tail-call SCCs.
* `reDMP1` PRX `funcs` (read-only): unchanged at `9,004` (`761,828` B, `92.5%`).
* Win64 Release build: **0 warnings**, CTest **32/32 passed**.
