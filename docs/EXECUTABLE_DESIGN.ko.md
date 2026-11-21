# 원본 실행 파일 설계 요약 (DJ Max Portable 2, `ULKS-46116` v1.09)

이 문서는 원본 실행 파일 분석을 통해 확인한 구조적 사실을 누적 정리한다. 세부 증거는 `docs/analysis/`의 주제별 문서를 참조한다.

---

## 1. 실행 파일 신원 및 메모리 배치 (**확인됨**)

* 대상 실행 파일: `PSP_GAME/SYSDIR/BOOT.BIN` (844,948 바이트, SHA-1 `1a5c904989d430588da9653acad4b6c5cdab2885`)
* 형식: ELF32 MIPS Little-Endian, **`ET_EXEC` (`e_type = 2`)** — 고정 주소 링크(재배치 테이블 비어 있음)
* 단일 `PT_LOAD` 세그먼트:
  * `p_vaddr = 0x08804000`
  * `p_filesz = 0xCDF20` (843,552 바이트)
  * `p_memsz = 0x626500` (6,448,384 바이트, BSS 약 5.34 MiB 포함)
* 모듈 정보 (`.rodata.sceModuleInfo` @ `0x088C0D34`):
  * 이름: `djmax`
  * 진입점 (`e_entry`): `0x0880FC4C`
  * `$gp`: `0x088D9F10`
  * exports: `0x088C0AFC..0x088C0B0C` (2개)
  * imports: `0x088C0B14..0x088C0D30` (27개 라이브러리, 229개 NID)

---

## 2. 코드 규모 및 정적 재컴파일 현황 (**확인됨**)

* `.text`: `0x08804000` + `0xBC3D0` (771,024 바이트), import stub 영역 `0x088C03D0` + 1,832 바이트.
* `allegrexrecomp emit --trampoline` (고정 `ET_EXEC`이므로 `--base` 생략):
  * 발견 함수 `7,933`개, 내부 진입점 `27,936`개, `.text` 커버리지 `97.5%` (`753,592 / 772,856` 바이트).
  * 점프 테이블 `355`개 → `4,560`개 타깃 해석, 미해결 간접 점프 `0`건.
  * VFPU 명령 `84`개(`0.04%`), invalid 워드 `37`개(`0.02%`).

---

## 3. 펌웨어 인터페이스 및 자산 구조 (**확인됨** / **추정**)

* **확인됨**: 27개 펌웨어 라이브러리에서 229개 고유 NID를 임포트한다. `repsp` v0.0.30(`81558ef`) 기준으로 141개(61.6%)가 이미 구현되어 있으며, 88개(38.4%)가 미구현이다. 실제 부팅 시 `module_start`(`0x0880FC4C`) 직후 첫 호출 미구현 NID는 `SysMemUserForUser` `0x7591C7DB`(`sceKernelSetCompiledSdkVersion`)이다(세부 목록: [docs/analysis/boot.md](analysis/boot.md)).
* **확인됨**: DMP1과 달리 `USRDIR/modules/*.prx`가 디스크에 없고, 애드혹 멀티플레이(`sceNet`, `sceNetAdhoc`, `sceNetAdhocctl`, `sceNetAdhocMatching`, 합 23 NID), 리모컨(`sceHprm`, 2 NID), `sceImpose`(2 NID), `sceRtc`(1 NID)를 새로 임포트한다.
* **확인됨**: 자산은 `USRDIR/all.mp`..`all7.mp`(7개, 합 약 1.61 GB)와 `USRDIR/movie/*.pmf`(6개)로 구성된다.
* **미확정**: 런타임 스레드 구성, 메모리 파티션 사용량, `all*.mp` 컨테이너 구조, 게임플레이 메모리 구조(부팅 이후 후속 분석 예정).
