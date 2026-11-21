# 작업 지시: `ET_EXEC`의 코드 조립 함수 포인터 (issue #5)

설계: [20261121-i005](../design/20261121-i005-exec-pointer-scan.md). 브랜치: `i005-exec-pointer-scan`(상위 저장소). 커밋 제목 끝에 `(#5)`.

1. `patches/psprecomp/0007-exec-scan-code-pointers.patch`(설계 §2). 패치 적용 순서를 정하는 CMake(또는 스크립트)에 넣는다. `third_party/psprecomp` submodule은 고치지 않는다(패치는 빌드 디렉터리 사본에 적용된다).
2. `patches/psprecomp/README.md`: 패치 목록과 upstream 표에 0007(“#36에 반영 필요, 미결정”)을 추가한다.
3. 생성 코드 없이 Win64 Debug/Release 빌드와 CTest.
4. `regenerate.py`로 다시 lift, `lift_audit.py`. issue #1 값과 비교(설계 §3, §5의 2). reDMP1 `BOOT.BIN`의 `funcs` 함수 수 비교(설계 §5의 4, 읽기만).
5. 생성 코드 포함 Release 빌드, CTest, 헤드리스 실행(`--save-dir`/`--crash-dir`는 저장소 밖). `0x0880FD20` lift 여부와 다음 정지를 기록한다.
6. 문서: `docs/analysis/executable.md`(새 lift 수치), `docs/analysis/boot.md`(정지 지점 변화), 작업 로그 `docs/work-logs/20261121-i005-exec-pointer-scan.md`.
7. `git status`(게임 데이터·생성 코드 없음) 확인 후 커밋. `third_party/repsp`의 submodule 변경(#67 브랜치)은 이 issue의 커밋에 넣지 않는다.

금지: push, PR, 태그, issue. repsp·psprecomp·reDMP1 안의 수정. 업스트림 PR 작업.

---

# Work order: code-built function pointers in an `ET_EXEC` (issue #5)

Design: [20261121-i005](../design/20261121-i005-exec-pointer-scan.md). Branch: `i005-exec-pointer-scan` (superproject). Commit titles end with `(#5)`.

(1) `patches/psprecomp/0007-exec-scan-code-pointers.patch` (design §2), added wherever CMake (or the script) orders the patches; the `third_party/psprecomp` submodule is not edited (patches apply to the copy in the build directory). (2) `patches/psprecomp/README.md`: add 0007 to the patch list and the upstream table ("needs to go into #36, undecided"). (3) Win64 Debug/Release without generated code, with CTest. (4) Re-lift with `regenerate.py`, run `lift_audit.py`, compare with issue #1's values (design §3, §5 item 2); compare the `funcs` count of reDMP1's `BOOT.BIN` (design §5 item 4, read only). (5) Release with generated code, CTest, the headless run (`--save-dir`/`--crash-dir` outside the repository); record whether `0x0880FD20` is lifted and the next stop. (6) Documents: `docs/analysis/executable.md` (new lift numbers), `docs/analysis/boot.md` (the stop moving), the work log `docs/work-logs/20261121-i005-exec-pointer-scan.md`. (7) Check `git status` (no game data or generated code) and commit; the `third_party/repsp` submodule change (the #67 branch) does not go into this issue's commits.

Not allowed: push, pull request, tag, issue; edits inside repsp, psprecomp or reDMP1; upstream PR work.
