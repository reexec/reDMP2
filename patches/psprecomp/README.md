# psprecomp 패치 모음

이 디렉터리는 `third_party/psprecomp`(커밋 `14749c1`, upstream `v0.6.0`)에 적용하는 로컬 패치를 보관한다. 패치는 configure 때 빌드 디렉터리의 복사본에만 적용되고, submodule 자체는 바꾸지 않는다.

## upstream 상태

upstream이 받아들이면 해당 로컬 패치를 지우고 submodule을 올린다.

| 로컬 패치 | upstream | 상태 |
|---|---|---|
| `0001-relocate-prx-at-base.patch` | [sp00nznet/psprecomp#36](https://github.com/sp00nznet/psprecomp/pull/36) | v0.6.0에 들어감(삭제됨). 머지된 #36에도 `0007`이 고친 `ET_EXEC` 스캔 결함이 남아 있다(`analyze.c`의 `if (an->scan_calls && !an->image)`). |
| `0003-jalr-fallthrough.patch` | [sp00nznet/psprecomp#31](https://github.com/sp00nznet/psprecomp/pull/31)(다른 기여자) | v0.6.0에 들어감(삭제됨). |
| `0004-j-owned-target-goto.patch` | [sp00nznet/psprecomp#35](https://github.com/sp00nznet/psprecomp/pull/35) | v0.6.0에 들어감(삭제됨). |
| `0005-emit-trampoline.patch` | [sp00nznet/psprecomp#37](https://github.com/sp00nznet/psprecomp/pull/37) | 열림. reDMP1이 v0.6.0 위로 rebase한 판(`d9278d2`, #32의 `psp_call_guest_fn`/`psp_call_guest_addr` 형식)을 사용. PR의 `j` 경로 결함은 `0009`가 보완한다. |
| `0006-loop-hook.patch` | [sp00nznet/psprecomp#38](https://github.com/sp00nznet/psprecomp/pull/38) | v0.6.0에 들어감(삭제됨). |
| `0007-exec-scan-code-pointers.patch` | [sp00nznet/psprecomp#36](https://github.com/sp00nznet/psprecomp/pull/36) | #36은 이미 머지됨. v0.6.0 위로 재생성. 제출 보류(사용자 결정). |
| `0008-exec-scan-outside-text-bodies.patch` | 없음 (로컬 전용) | 미제출, 보류(사용자 결정). v0.6.0 위로 재생성. |
| `0009-tail-jump-import-return.patch` | [sp00nznet/psprecomp#37](https://github.com/sp00nznet/psprecomp/pull/37) | #37 후속 수정(`0005`는 PR #37 rebase 판 그대로 유지). v0.6.0 위로 재생성. 반영 보류(사용자 결정). |
| `0010-exclude-import-stubs-from-funcs.patch` | 없음 (로컬 전용) | 미제출, 보류(사용자 결정). v0.6.0 위로 재생성. |
| `0011-lui-pair-reg-tracking.patch` | 없음 (로컬 전용) | 미제출, 보류(사용자 결정). v0.6.0 위로 재생성. |

## 남은 패치

1. **`0005-emit-trampoline.patch`** — `emit`에 `--trampoline` 선택 사항을 추가해 비링크 외부 꼬리 호출을 `PSP_TAIL(...)` / `psp_dispatch_tail`로 내보내고, 게스트 호출은 v0.6.0(#32)의 `psp_call_guest_fn`/`psp_call_guest_addr`로 연결한다.
2. **`0007-exec-scan-code-pointers.patch`** — `ET_EXEC` 이미지에서 `.text` `lui+addiu` 포인터 스캔이 `an->image` 존재로 생략되는 문제를 고친다(`skip_lui_scan` 플래그로 PRX 이미지에서만 생략).
3. **`0008-exec-scan-outside-text-bodies.patch`** — `a_discover` 고정점 루프에서 `.text` 바깥에 이미 발견된 함수 본문(`.data` 내 C++ 정적 초기화자 등)의 `lui+addiu`/`ori` 쌍만 추가로 스캔해 `.text` 진입점으로 승격한다(`skip_lui_scan == 0`일 때만).
4. **`0009-tail-jump-import-return.patch`** — `--trampoline` 모드에서 `j <import>` 꼬리 호출이 `is_import`와 `is_function`이 동시에 참일 때 `return;`을 생략해 다음 레이블로 폴스루하는 결함을 고친다.
5. **`0010-exclude-import-stubs-from-funcs.patch`** — `trace_function()`(`in.is_branch`/`in.is_jump`)과 `a_discover()`(초기 시드·재탐색 타깃)에서 `is_import_stub()`을 걸러 `.sceStub.text` 주소가 `an->funcs`에 들어가지 않도록 하고, `emit.c`의 `psp_recomp_register()`에 모든 임포트 스텁을 `psp_import_*`로 등록해 간접 호출(`jalr`)이 HLE에 도달하도록 한다.
6. **`0011-lui-pair-reg-tracking.patch`** — `analyze.c`의 `.text` 및 `0008` 비-`.text` 본문 `lui+addiu`/`ori` 스캔이 공유하는 `scan_lui_pairs()`를 도입해 고정 4명령 창을 레지스터 추적(`rX` 덮어쓰기, 분기·점프의 지연 슬롯 이후, 또는 최대 16명령에서 종료; 그 사이 `rs == rX`인 모든 `addiu`/`ori` 채택)으로 교체한다(`skip_lui_scan == 0`인 `ET_EXEC` 전용).

---

# psprecomp patches

This directory keeps the local patches applied to `third_party/psprecomp` (commit `14749c1`, upstream `v0.6.0`). They are applied at configure time to a copy in the build directory; the submodule itself is not modified.

## Upstream status

When upstream accepts a patch, the matching local patch is removed and the submodule moved forward.

| Local patch | Upstream | Status |
|---|---|---|
| `0001-relocate-prx-at-base.patch` | [sp00nznet/psprecomp#36](https://github.com/sp00nznet/psprecomp/pull/36) | In v0.6.0 (removed). Merged #36 still carries the `ET_EXEC` scan defect `0007` fixes (`analyze.c`: `if (an->scan_calls && !an->image)`). |
| `0003-jalr-fallthrough.patch` | [sp00nznet/psprecomp#31](https://github.com/sp00nznet/psprecomp/pull/31) (another contributor) | In v0.6.0 (removed). |
| `0004-j-owned-target-goto.patch` | [sp00nznet/psprecomp#35](https://github.com/sp00nznet/psprecomp/pull/35) | In v0.6.0 (removed). |
| `0005-emit-trampoline.patch` | [sp00nznet/psprecomp#37](https://github.com/sp00nznet/psprecomp/pull/37) | Open; uses reDMP1's v0.6.0 rebase (`d9278d2`, emitting #32's `psp_call_guest_fn`/`psp_call_guest_addr`). The `j` path defect is covered by `0009`. |
| `0006-loop-hook.patch` | [sp00nznet/psprecomp#38](https://github.com/sp00nznet/psprecomp/pull/38) | In v0.6.0 (removed). |
| `0007-exec-scan-code-pointers.patch` | [sp00nznet/psprecomp#36](https://github.com/sp00nznet/psprecomp/pull/36) | #36 already merged; regenerated onto v0.6.0; submission on hold (user decision). |
| `0008-exec-scan-outside-text-bodies.patch` | None (local) | Not submitted, on hold (user decision); regenerated onto v0.6.0. |
| `0009-tail-jump-import-return.patch` | [sp00nznet/psprecomp#37](https://github.com/sp00nznet/psprecomp/pull/37) | Follow-up to #37 (`0005` kept equal to the rebased PR #37); regenerated onto v0.6.0; on hold (user decision). |
| `0010-exclude-import-stubs-from-funcs.patch` | None (local) | Not submitted, on hold (user decision); regenerated onto v0.6.0. |
| `0011-lui-pair-reg-tracking.patch` | None (local) | Not submitted, on hold (user decision); regenerated onto v0.6.0. |
