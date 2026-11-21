# psprecomp 패치 모음

이 디렉터리는 `third_party/psprecomp`(커밋 `5ca18fa`, upstream `main`)에 적용하는 로컬 패치를 보관한다. 패치는 configure 때 빌드 디렉터리의 복사본에만 적용되고, submodule 자체는 바꾸지 않는다. 이 패치 모음은 [reexec/reDMP1](https://github.com/reexec/reDMP1)에서 그대로 가져왔다.

## upstream 상태

upstream이 받아들이면 해당 로컬 패치를 지우고 submodule을 올린다.

| 로컬 패치 | upstream | 상태 |
|---|---|---|
| `0001-relocate-prx-at-base.patch` | [sp00nznet/psprecomp#36](https://github.com/sp00nznet/psprecomp/pull/36) | 제출함. 로컬 패치는 PR의 커밋과 같다(테스트 파일만 뺐다). |
| `0003-jalr-fallthrough.patch` | [sp00nznet/psprecomp#31](https://github.com/sp00nznet/psprecomp/pull/31)(다른 기여자) | 같은 수정이 이미 제출돼 있어 로컬 패치는 #31의 조건(`jalr $zero`는 끝으로 본다)에 맞췄다. |
| `0004-j-owned-target-goto.patch` | [sp00nznet/psprecomp#35](https://github.com/sp00nznet/psprecomp/pull/35) | 제출함. 로컬 패치는 PR의 커밋과 같다(테스트 파일만 뺐다). |
| `0005-emit-trampoline.patch` | [sp00nznet/psprecomp#37](https://github.com/sp00nznet/psprecomp/pull/37) | 제출함. 이 저장소의 런타임은 `src/recomp_abi/`에 있다. |
| `0006-loop-hook.patch` | [sp00nznet/psprecomp#38](https://github.com/sp00nznet/psprecomp/pull/38) | 제출함. |
| `0007-exec-scan-code-pointers.patch` | [sp00nznet/psprecomp#36](https://github.com/sp00nznet/psprecomp/pull/36) | #36에 반영 필요, 미결정. |
| `0008-exec-scan-outside-text-bodies.patch` | 없음 (로컬 전용) | 미제출 (`.text` 밖 발견 함수 본문의 `lui+addiu` 스캔). |
| `0009-tail-jump-import-return.patch` | [sp00nznet/psprecomp#37](https://github.com/sp00nznet/psprecomp/pull/37) | #37 후속 수정 (`0005`는 PR #37 그대로 유지). |
| `0010-exclude-import-stubs-from-funcs.patch` | 없음 (로컬 전용) | 미제출 (임포트 스텁을 `an->funcs`에서 제외하고 디스패치 테이블에 `psp_import_*`로 등록). |

## 패치

1. **`0001-relocate-prx-at-base.patch`** — PRX를 지정한 주소에 재배치해 옮긴다(`prx_relocate()`, `--base <addr>`, `relocate <file> <base> <out>`).
2. **`0003-jalr-fallthrough.patch`** — 본문이 연결하는 `jalr`로 끝나면 다음 명령으로 이어지는 흐름을 낸다. `jr`과 `jalr $zero`는 끝으로 본다.
3. **`0004-j-owned-target-goto.patch`** — 현재 본문이 가진 주소로 가는 `j`는 함수 진입점이라도 `goto`로 옮긴다(뒤로 가면 `PSP_LOOP`).
4. **`0005-emit-trampoline.patch`** — `emit`에 `--trampoline` 선택 사항을 추가해 비링크 외부 꼬리 호출을 `PSP_TAIL(...)` / `psp_dispatch_tail`로 내보낸다.
5. **`0006-loop-hook.patch`** — `emit.c`가 내보내는 헤더에 `#ifdef PSPRECOMP_LOOP_HOOK` 분기를 추가해 `PSP_LOOP(a)`를 `psp_loop_hook(a)`로 연결한다.
6. **`0007-exec-scan-code-pointers.patch`** — `0001`이 `an->image` 존재 시 `.text` `lui+addiu` 포인터 스캔을 일괄 생략해 `ET_EXEC`에서 코드 조립 함수 포인터가 누락되는 문제를 고친다(`skip_lui_scan` 플래그로 PRX 이미지에서만 생략).
7. **`0008-exec-scan-outside-text-bodies.patch`** — `a_discover` 고정점 루프에서 `.text` 바깥에 이미 발견된 함수 본문(`.data` 내 C++ 정적 초기화자 등)의 `lui+addiu`/`ori` 쌍만 추가로 스캔해 `.text` 진입점으로 승격한다(`skip_lui_scan == 0`일 때만).
8. **`0009-tail-jump-import-return.patch`** — `--trampoline` 모드에서 `j <import>` 꼬리 호출이 `is_import`와 `is_function`이 동시에 참일 때 `return;`을 생략해 다음 레이블로 폴스루하는 결함을 고친다.
9. **`0010-exclude-import-stubs-from-funcs.patch`** — `trace_function()`(`in.is_branch`/`in.is_jump`)과 `a_discover()`(초기 시드·재탐색 타깃)에서 `is_import_stub()`을 걸러 `.sceStub.text` 주소가 `an->funcs`에 들어가지 않도록 하고, `emit.c`의 `psp_recomp_register()`에 모든 임포트 스텁을 `psp_import_*`로 등록해 간접 호출(`jalr`)이 HLE에 도달하도록 한다.

---

# psprecomp patches

This directory keeps the local patches applied to `third_party/psprecomp` (commit `5ca18fa`, upstream `main`). They are applied at configure time to a copy in the build directory; the submodule itself is not modified. Carried over from [reexec/reDMP1](https://github.com/reexec/reDMP1).

## Upstream status

When upstream accepts a patch, the matching local patch is removed and the submodule moved forward.

| Local patch | Upstream | Status |
|---|---|---|
| `0001-relocate-prx-at-base.patch` | [sp00nznet/psprecomp#36](https://github.com/sp00nznet/psprecomp/pull/36) | Submitted; matches the PR commit minus the test file. |
| `0003-jalr-fallthrough.patch` | [sp00nznet/psprecomp#31](https://github.com/sp00nznet/psprecomp/pull/31) (another contributor) | Matches #31's condition (`jalr $zero` ends a body). |
| `0004-j-owned-target-goto.patch` | [sp00nznet/psprecomp#35](https://github.com/sp00nznet/psprecomp/pull/35) | Submitted; matches the PR commit minus the test file. |
| `0005-emit-trampoline.patch` | [sp00nznet/psprecomp#37](https://github.com/sp00nznet/psprecomp/pull/37) | Submitted; this repository's runtime is in `src/recomp_abi/`. |
| `0006-loop-hook.patch` | [sp00nznet/psprecomp#38](https://github.com/sp00nznet/psprecomp/pull/38) | Submitted. |
| `0007-exec-scan-code-pointers.patch` | [sp00nznet/psprecomp#36](https://github.com/sp00nznet/psprecomp/pull/36) | Needs to go into #36, undecided. |
| `0008-exec-scan-outside-text-bodies.patch` | None (local) | Not submitted (scans discovered non-`.text` function bodies for `lui+addiu` pointers when `skip_lui_scan == 0`). |
| `0009-tail-jump-import-return.patch` | [sp00nznet/psprecomp#37](https://github.com/sp00nznet/psprecomp/pull/37) | Follow-up to #37 (`0005` kept equal to PR #37); always emit `return;` after `j <import>`. |
| `0010-exclude-import-stubs-from-funcs.patch` | None (local) | Not submitted (filters `is_import_stub()` from `an->funcs` and registers every `an->imports[i]` as `psp_import_*` in `psp_recomp_register()` for indirect calls). |
