# 링크 디스크(UMD 교체) 흐름 분석

설계 문서 [20261124-i023-link-disc.md](../design/20261124-i023-link-disc.md) 3절(1단계)에 따라, DMP2(`ULKS-46116`) 원본 실행 파일의 생성 코드(`src/game/dmp2/recomp/generated/`), 디스크 덤프(`roms/ULKS46116/`, `roms/ULKS46050/`), 그리고 `build/win64-release-ffmpeg` 실측 실행으로 링크 디스크 흐름을 분석한 결과다.

각 사실은 **확인됨**(생성 코드·바이너리·실측으로 확인), **추정**(정적 해석 또는 외부 문서 기준이며 실측 미완료), **미확정**(교체 모델 구현 뒤 확인 필요)으로 구분한다.

---

## 요약: 2단계(repsp #79)와 3단계(reDMP2)에 영향을 주는 핵심 발견

1. **교체 진입 전 미구현 HLE 정지 지점 확인됨**: 타이틀에서 `LINK DISC` → `예`를 누르면 UMD 교체 단계에 닿기 **전에** `sceUtilitySavedata` 확장 모드 **`mode 12`**(`0x088BD074`)에서 정지한다(`Unimplemented sceUtilitySavedata mode 12`, 종료 코드 `11`, vblank `1419`).
2. **링크 디스크 경로에서 추가로 필요한 미구현 HLE 확인됨**:
   * `sceUtilitySavedata` 확장 모드 6종: `mode 12`(`0x088BD074`), `mode 13`(`0x088BD49C`), `mode 15`(`0x088BD7A8`), `mode 17`(`0x088BD688`), `mode 19`(`0x088BD8CC`), `mode 22`(`0x088BD3B0`). 현재 repsp는 `mode 0..8`만 구현되어 있다.
   * `IoFileMgrForUser::sceIoDevctl`(`0x54F5FB11`, 스텁 `0x088C03E0`): `ms0:` 여유 공간 조회(명령 `0x02425818`, `0x0886E1D8`).
   * `sceSuspendForUser::sceKernelVolatileMemTryLock`(`0xA14F40B2`, 스텁 `0x088C09F0`) 및 `sceKernelVolatileMemUnlock`(`0xA569E425`, 스텁 `0x088C09F8`): `linkdisc_{kr,en,jp}.mp`를 휘발성 메모리에 적재(`0x0886E14C`).
   * `sceUmdUser`: `sceUmdCheckMedium`(`0x46EBB729`, `0x088C0A08`), `sceUmdReplacePermit`(`0xCBE9F02A`, `0x088C0A40`), `sceUmdReplaceProhibit`(`0x87533940`, `0x088C0A28`).
3. **교체된 DMP1 디스크에서 `PARAM.SFO`뿐 아니라 `all.mp`·`all2.mp`도 열어 읽음(확인됨)**: 설계 2절의 기존 추정("`PARAM.SFO`만 읽는다")과 달리, DMP2는 `ULKS46050` 확인 직후 `0x0886EB94` → `0x08815100`으로 교체된 DMP1 UMD의 `all.mp`와 `all2.mp`를 실제로 마운트한다. DMP1 덤프(`roms/ULKS46050/PSP_GAME/USRDIR/`)에도 `all.mp`와 `all2.mp`가 존재한다.
4. **DMP2는 UMD 교체 허용 전에 열린 아카이브 핸들을 스스로 닫음(확인됨)**: `CLinkDiscScene::Init`(`0x0886D89C`)의 `0x0886EC38`과 DMP2 복귀 시 `0x0886ECA8`이 `0x08815164`로 열려 있던 `all.mp`/`all2.mp` 핸들을 먼저 닫고 교체 뒤 다시 연다.
5. **`ms0:/PSP/SAVEDATA`와 `sceUtility` 저장 경로가 현재 분리되어 있음(확인됨)**: `src/app/main.cpp`에서 `ms0:`은 `<user_data>/ms0`에, `sceUtility`는 `--save-dir`(기본 `<user_data>/savedata`)에 매핑된다. 링크 디스크는 `sceUtilitySavedata`로 쓴 `ULKS46116DATA0/linkdisc_game.mp`를 플레이 장면에서 `ms0:/PSP/SAVEDATA/ULKS46116DATA0/linkdisc_game.mp`로 직접 읽으므로 두 경로를 일치시켜야 한다.

---

## 1. 링크 디스크 메뉴 위치, 안내 문구 및 버튼 순서

* **상태**: 진입·확인 창까지 **확인됨**(실측 BMP 및 생성 코드). 교체 안내 창 문구 자체는 **미확정**(교체 모델 및 선행 HLE 뒤 확인).
* **타이틀 화면 캐러셀 위치**(`0x0884EFA0`에서 6개 배경 로드 확인됨):
  `PORTABLE 2` (`title_bg_p2.vce`) → `NETWORK BATTLE` (`title_bg_network.vce`) → `COLLECTION` (`title_bg_collection.vce`) → **`LINK DISC`** (`title_bg_linkdisc.vce`, 보라색 배경 + 녹색 원형 화살표 로고) → `MV edition` (`title_bg_mvedition.vce`) → `O.S.T` (`title_bg_ost.vce`) → `PORTABLE 2`로 순환.
* **진입 버튼 순서**(실측 `build/win64-release-ffmpeg` 헤드리스 검증):
  1. 타이틀 초기 위치(`PORTABLE 2`)에서 `RIGHT` 3회 → `LINK DISC` 선택.
  2. `START` 또는 `CIRCLE` → 확인 대화상자 표시:
     * 문구: `"본 모드는 전작인 [ 디제이맥스 포터블 ] 디스크가 필요합니다. 계속 진행하시겠습니까?"`
     * 선택지: `예` / `아니오` (기본값 `예`).
  3. `예`에서 `CIRCLE` → `CLinkDiscScene`(`0x0886ED7C`, 장면 팩토리 `0x08852404`) 생성 및 `Init`(`0x0886D89C`) 실행.
  4. **현재 실측 정지 지점**: `Init` 직후 `0x088BCE58` → `0x088BD074`에서 `sceUtilitySavedataInitStart(mode=12)` 호출 → `Reason: Unimplemented sceUtilitySavedata mode 12` (종료 코드 `11`, vblank `1419`).
* **교체 이후 화면 흐름**(생성 코드 정적 확인됨, 실측은 **교체 모델 뒤 확인**):
  * 메시지 ID(`0x088331D0`, `this+11792`): `6`(전작 디스크 삽입 요청), `9`(잘못된 디스크 — DMP1/DMPI가 아님), `10`(링크 디스크 종료 확인), `11`(DMP2 디스크 재삽입 요청), `12`(잘못된 디스크 — DMP2가 아님).
  * DMP1 삽입 성공 시 `system/title/title_portable1_intro.vce` → `title_portable1_wait.vce`와 `sound/mode/title.at3`가 재생되며(상태 `7`→`8`→`9`), 상태 `9`(`0x0886E608`)에서 `START`/`CIRCLE`을 누르면 **DMP1 디스크가 꽂힌 상태로** 장면 `12`(모드·곡 선택)로 진입한다. `CROSS`를 누르면 메시지 `10` 확인 후 DMP2 재삽입 대기(상태 `6`)로 넘어간다.

---

## 2. UMD 함수 호출 순서, 상태 머신 및 기대값

* **상태**: 호출 순서와 비트 조건은 생성 코드로 **확인됨**. 실제 교체 응답은 **교체 모델 뒤 확인**.

### 2.1 관련 스텁과 repsp 구현 현황

| 모듈 | 함수 | NID | DMP2 스텁 | repsp 현황 |
|---|---|---|---:|---|
| `sceUmdUser` | `sceUmdCheckMedium` | `0x46EBB729` | `0x088C0A08` | **미구현** |
| `sceUmdUser` | `sceUmdWaitDriveStatWithTimer` | `0x56202973` | `0x088C0A10` | 구현됨(정적) |
| `sceUmdUser` | `sceUmdCancelWaitDriveStat` | `0x6AF9B50A` | `0x088C0A18` | 구현됨(정적) |
| `sceUmdUser` | `sceUmdGetDriveStat` | `0x6B4A146C` | `0x088C0A20` | 구현됨(정적) |
| `sceUmdUser` | `sceUmdReplaceProhibit` | `0x87533940` | `0x088C0A28` | **미구현** |
| `sceUmdUser` | `sceUmdWaitDriveStat` | `0x8EF08FCE` | `0x088C0A30` | 구현됨(정적) |
| `sceUmdUser` | `sceUmdActivate` | `0xC6183D47` | `0x088C0A38` | 구현됨(정적) |
| `sceUmdUser` | `sceUmdReplacePermit` | `0xCBE9F02A` | `0x088C0A40` | **미구현** |
| `sceUmdUser` | `sceUmdDeactivate` | `0xE83742BA` | `0x088C0A48` | 구현됨(정적) |
| `sceImpose` | `sceImposeSetUMDPopup` | `0x72189C48` | `0x088C0AE0` | 구현됨 |
| `IoFileMgrForUser` | `sceIoDevctl` | `0x54F5FB11` | `0x088C03E0` | **미구현** |
| `sceSuspendForUser` | `sceKernelVolatileMemTryLock` | `0xA14F40B2` | `0x088C09F0` | **미구현** |
| `sceSuspendForUser` | `sceKernelVolatileMemUnlock` | `0xA569E425` | `0x088C09F8` | **미구현** |
| `sceUtility` | `sceUtilitySavedataInitStart` (`mode 12,13,15,17,19,22`) | `0x50C4CD57` | `0x088C0A68` | `mode 0..8`만 구현 |

### 2.2 `CLinkDiscScene` 상태 머신(`0x0886D994`, 점프 테이블 `0x088C5A18`)

```mermaid
stateDiagram-v2
    [*] --> Init : CLinkDiscScene::Init (0x0886D89C)
    Init --> State1 : all.mp/all2.mp 닫기(0x0886EC38)\nsavedata 조회(mode 12)
    State1 --> State2 : linkdisc 파일 없음
    State1 --> State4 : linkdisc 파일 이미 있음
    State2 --> State3 : ms0 여유 3MiB 확인(sceIoDevctl)\nlinkdisc_{kr,en,jp}.mp 저장
    State3 --> State4 : ms0 여유 900KiB 확인(sceIoDevctl)\nlinkdisc_game.mp 저장
    State4 --> State5 : 휘발성 메모리 복사(0x0886E14C)\nsceImposeSetUMDPopup(0)\nsceUmdReplacePermit() + msg 6
    State5 --> State5 : 폴링: !(GetDriveStat & 0x20)\n15초 경과 시 CheckMedium()==0 → msg 6
    State5 --> State5 : GetDriveStat & 0x20\nPARAM.SFO != DMP1/DMPI → msg 9
    State5 --> State7 : PARAM.SFO == ULKS46050 / ULKS46059\nReplaceProhibit + Popup(1)\ndiscstock 로드 + DMP1 all.mp/all2.mp 열기
    State7 --> State8 : title_portable1_intro.vce
    State8 --> State9 : title_portable1_wait.vce
    State9 --> Scene12 : START/CIRCLE (DMP1 꽂힌 채 플레이)
    State9 --> State6 : CROSS + msg 10 확인\nPopup(0) + ReplacePermit() + msg 11
    State6 --> State6 : 폴링: !(GetDriveStat & 0x20)\n15초 경과 시 CheckMedium()==0 → msg 12
    State6 --> State6 : GetDriveStat & 0x20\nPARAM.SFO != ULKS46116 → msg 12
    State6 --> State10 : PARAM.SFO == ULKS46116\nReplaceProhibit + Popup(1)\nDMP1 닫기 + DMP2 all.mp/all2.mp 재오픈
    State10 --> [*] : 타이틀 장면(24) 복귀
```

### 2.3 세부 호출 규칙과 기대값

* **`ReplacePermit` / `ReplaceProhibit` 호출 시점**(확인됨):
  * **허용**: DMP1 삽입 요청 직전(`0x0886E9B8`: `sceImposeSetUMDPopup(0)` → `sceUmdReplacePermit()`), DMP2 재삽입 요청 직전(`0x0886EA14`: `sceImposeSetUMDPopup(0)` → `sceUmdReplacePermit()`). 잘못된 디스크로 재시도할 때도 대화상자 콜백(`0x0886E840`)이 두 함수를 다시 부른다.
  * **금지**: `0x0886EA94`로 올바른 디스크를 확인한 직후(`0x0886EA70`: `sceUmdReplaceProhibit()` → `sceImposeSetUMDPopup(1)`).
* **`sceUmdGetDriveStat` 검사 비트**(확인됨):
  * 상태 `5`(`0x0886DCE0`)와 상태 `6`(`0x0886DE40`) 모두 반환값에 `andi $v0, $v0, 0x20`(**`PSP_UMD_READY` = `0x20`**)만 검사한다.
  * 다른 비트(`PSP_UMD_NOT_PRESENT`, `PSP_UMD_PRESENT`, `PSP_UMD_CHANGED` 등)는 직접 비교하지 않는다. 즉, 디스크가 꺼내지거나 교체 중일 때는 `0x20` 비트가 꺼져 있어야 하고, 새 디스크가 준비되면 `0x20` 비트가 켜져야 한다.
* **`sceUmdCheckMedium` 반환값 의미**(확인됨):
  * 상태 `5`(`0x0886DE0C`)와 상태 `6`(`0x0886DEC8`)에서 `!(GetDriveStat() & 0x20)` 상태가 **15,000 ms(15초)** 이상 지속될 때만 호출한다(`0x0886DDF0..0x0886DE00`, `sltiu $v0, $v0, 15000`).
  * 반환값이 **`0`**이면 "디스크가 없음"으로 보고 삽입 안내 대화상자(상태 `5`는 msg `6`, 상태 `6`은 msg `12`)를 다시 띄운다. **`0`이 아니면** 디스크가 들어와 준비 중으로 간주하고 폴링을 이어간다.
* **교체 대기 방식**(확인됨):
  * `CLinkDiscScene` 자체는 `WaitDriveStat*`를 쓰지 않고 **매 프레임 `sceUmdGetDriveStat()` 폴링**으로 대기한다.
  * 별도의 파일 입출력 재시도 계층(`0x0882E484..0x0882E5C0`)은 `!(sceUmdGetDriveStat() & 0x20)`일 때 `sceUmdWaitDriveStatWithTimer(0x20, 10000000)`(10초 타임아웃) 또는 `sceUmdWaitDriveStat(0x20)`을 호출한다.
* **열린 파일 처리**(확인됨):
  * `CLinkDiscScene::Init`(`0x0886EC38`)이 교체 허용 전에 `0x08815164`로 DMP2의 `all.mp`·`all2.mp`를 닫는다.
  * DMP2 복귀 시(`0x0886ECA8`)에도 `0x08815164`로 DMP1의 `all.mp`·`all2.mp`를 먼저 닫고 `0x0881506C`로 DMP2의 `all.mp`·`all2.mp`를 다시 연다.

---

## 3. 교체된 디스크(DMP1)에서 읽는 파일

* **상태**: 정적 분석 **확인됨**, 실제 파일 입출력은 **교체 모델 뒤 확인**.

1. **`disc0:/PSP_GAME/PARAM.SFO`** (`0x0886EA94`):
   * 오프셋 `296`에서 16바이트를 읽어 디스크 ID를 비교한다.
   * 실제 덤프 확인됨:
     * `roms/ULKS46116/PSP_GAME/PARAM.SFO` (472 B) 오프셋 296 = `ULKS46116\0...` → 반환 `2`
     * `roms/ULKS46050/PSP_GAME/PARAM.SFO` (472 B) 오프셋 296 = `ULKS46050\0...` → 반환 `0`
     * `ULKS46059` → 반환 `1`
     * 그 외 → 반환 `3`
2. **`disc0:/PSP_GAME/USRDIR/all.mp` 및 `disc0:/PSP_GAME/USRDIR/all2.mp`** (`0x0886EB94` → `0x08815100`):
   * `0x0886EB94`가 기존 아카이브를 닫고(`0x08815164`), `0x08815100`이 문자열 `0x088C7BEC`(`"all.mp"`)과 `0x088C7BF4`(`"all2.mp"`)를 열어 교체된 DMP1 UMD의 아카이브를 마운트한다.
   * DMP1 덤프(`roms/ULKS46050/PSP_GAME/USRDIR/`)에 실제로 `all.mp`와 `all2.mp`가 존재한다(**확인됨**).
   * 따라서 링크 디스크 플레이 중 DMP1의 곡·채보·오디오·BGA는 교체된 DMP1 UMD의 `all.mp`·`all2.mp`에서 직접 스트리밍되고, DMP2 엔진/UI 자원은 메모리 스틱과 휘발성 메모리에 미리 꺼내 둔 `linkdisc_*.mp`에서 읽는 구조다(**추정**, 교체 모델 뒤 확인).

---

## 4. `linkdisc_*.mp` 생성 시점·크기·원본과 세이브(`SD.BIN`)

* **상태**: `linkdisc_*.mp` 추출 시점·크기·원본은 **확인됨**. 세이브(`SD.BIN`) 기록 여부는 **미확정**(교체 모델 뒤 확인).

* **쓰는 시점**: DMP2로 되돌린 뒤가 아니라, **UMD 교체 전**(상태 `1`~`3`, 아직 DMP2 UMD가 꽂혀 있는 시점)에 DMP2 아카이브에서 메모리 스틱(`ULKS46116DATA0`)으로 꺼내 둔다(**확인됨**).
  * 상태 `1`(`0x0886D9F0`): `0x088BCE58` → `0x088BD074`(`sceUtilitySavedata mode 12`)로 기존 파일 존재 여부를 확인한다. 이미 있으면 상태 `2`~`3`을 건너뛰고 상태 `4`로 간다.
  * 상태 `2`(`0x0886DA70`): `sceIoDevctl("ms0:", 0x02425818, ...)`로 여유 공간 `0x00300000`(3 MiB)을 확인한 뒤, 현재 언어에 맞는 `linkdisc_{kr,en,jp}.mp`를 `sceUtilitySavedata`로 쓴다.
  * 상태 `3`(`0x0886DB58`): 여유 공간 `0x000E1000`(900 KiB)을 확인한 뒤, `linkdisc_game.mp`를 `sceUtilitySavedata`로 쓴다.
* **원본(DMP2 아카이브 항목)과 크기**(`0x088BE3E8` / `BOOT.BIN` 문자열 확인됨):

  | DMP2 아카이브 경로 | 크기 (hex) | 크기 (bytes) | 용도 |
  |---|---:|---:|---|
  | `system/linkdisc_kr.mp` | `0x0022ABAD` | 2,272,173 | 한국어 UI/리소스 (상태 4에서 휘발성 메모리로 복사) |
  | `system/linkdisc_en.mp` | `0x0023277A` | 2,303,866 | 영어 UI/리소스 |
  | `system/linkdisc_jp.mp` | `0x00233B79` | 2,308,985 | 일본어 UI/리소스 |
  | `system/linkdisc_game.mp` | `0x00076404` | 484,356 | 플레이 장면(`0x08853628`, `0x0885462C`, `0x0885479C`)이 `ms0:/PSP/SAVEDATA/ULKS46116DATA0/linkdisc_game.mp`로 직접 열람 |

* **`sceUtilitySavedata` 확장 모드 호출 매핑**(`0x088BD074`~`0x088BD8CC` 확인됨, PSPSDK 모드 이름은 **추정**):
  * `0x088BD074`: `mode = 12` (PSPSDK `PSP_UTILITY_SAVEDATA_TYPE_FILES`, 파일 목록 조회)
  * `0x088BD49C`: `mode = 13` (`MAKEDATASECURE`, 보안 파일 생성)
  * `0x088BD7A8`: `mode = 15` (`READDATASECURE`, 보안 파일 읽기)
  * `0x088BD688`: `mode = 17` (`WRITEDATASECURE`, 보안 파일 쓰기)
  * `0x088BD8CC`: `mode = 19` (`ERASESECURE`, 보안 파일 삭제)
  * `0x088BD3B0`: `mode = 22` (`GETSIZE`, 크기 조회)
* **DMP2로 되돌린 뒤 하는 일 및 `SD.BIN` 기록**:
  * 상태 `6`에서 `ULKS46116`이 확인되면 `0x0886E0A4`가 링크 디스크 리소스를 정리하고 `0x0886ECA8`로 DMP1 아카이브를 닫은 뒤 DMP2 `all.mp`·`all2.mp`를 다시 열어 타이틀(장면 `24`)로 복귀한다(**확인됨**).
  * `0x0886E0A4` 자체에는 `SD.BIN`을 바로 저장하는 호출이 없으며, 링크 디스크 플레이 중 얻은 기록/해금이 일반 게임 세이브(`SD.BIN`)의 어느 필드에 저장되는지는 **미확정**(**교체 모델 뒤 확인**).

---

## 5. 해금되는 내용(`discstock_dmp1.csv`)

* **상태**: 호출 경로는 **확인됨**, 실제 곡·모드 목록은 **미확정**(교체 모델 뒤 확인).
* 상태 `5`에서 `ULKS46050`(DMP1)이 확인되면 `0x0886EA70` 직후, `0x0886EB94`(DMP1 아카이브 마운트) **직전에** `0x0886E7BC(this, 0)`을 부른다(`ULKS46059`는 인자 `1`로 `discstock_dmpi.csv`).
* `0x0886E7BC`는 `0x088295B4`를 호출해 전역 테이블 `0x08920BBC`(크기 `3972` 바이트)를 0으로 초기화한 뒤 `discstock_dmp1.csv`를 파싱해 채운다(**확인됨**).
* `discstock_dmp1.csv`는 암호화된 `MP02` 아카이브 안에 있어 정적 텍스트로 직접 꺼내지 않았다. 구체적인 곡 수·모드 목록과 영구 해금 여부는 **교체 모델 뒤 확인**한다.

---

## 6. `ms0:/PSP/SAVEDATA/...` 직접 접근과 `sceUtility` 세이브 경로(`--save-dir`)

* **상태**: **확인됨**(`src/app/main.cpp` 코드 확인).
* 현재 경로 불일치:
  * `ms0:` VFS 마운트(`src/app/main.cpp` 644, 698행): `repsp::platform::GetUserDataDirectory("redmp2") / "ms0"`로 고정되어 `--save-dir`를 따르지 않는다.
  * `sceUtility` `save_root`(`src/app/main.cpp` 841~843행): `--save-dir`가 있으면 그 경로, 없으면 `GetUserDataDirectory("redmp2") / "savedata"`를 사용한다.
* **영향**:
  * 링크 디스크 상태 `2`~`3`은 `sceUtilitySavedata`로 `<save_root>/ULKS46116DATA0/linkdisc_game.mp`를 생성한다.
  * 플레이 장면(`0x08853628`, `0x0885462C`, `0x0885479C`)은 포맷 문자열 `"/PSP/SAVEDATA/%sDATA0/linkdisc_game.mp"`(`0x088CB3DA`)로 `IoFileMgrForUser`를 통해 `ms0:/PSP/SAVEDATA/ULKS46116DATA0/linkdisc_game.mp`를 직접 연다.
  * 두 경로가 다르므로 현재 구조에서는 `linkdisc_game.mp`를 찾지 못한다.
* **3단계 반영 방향**:
  * `ms0:/PSP/SAVEDATA`와 `sceUtility` `save_root`가 항상 같은 디렉터리를 가리키도록 통합해야 한다(예: `ms0:` 루트 아래 `PSP/SAVEDATA`를 기본 `save_root`로 두고, `--save-dir` 지정 시 `ms0:/PSP/SAVEDATA` 마운트 또는 `ms0:` 경로가 그 위치를 가리키게 연결). 기존 `<user_data>/savedata`에 세이브가 있는 사용자를 위한 마이그레이션 절차는 3단계 설계에서 정한다.

---
---

# Link Disc (UMD Swap) Flow Analysis

Following Section 3 (Phase 1) of [20261124-i023-link-disc.md](../design/20261124-i023-link-disc.md), this document analyzes the Link Disc flow in DMP2 (`ULKS-46116`) from generated recompiled code (`src/game/dmp2/recomp/generated/`), disc dumps (`roms/ULKS46116/`, `roms/ULKS46050/`), and live measurements on `build/win64-release-ffmpeg`.

Statements are marked **confirmed** (verified in generated code, binaries, or live runs), **inferred** (static interpretation or external docs, not yet verified live), or **unresolved** (to confirm after the swap model).

---

## Summary: Key Findings Affecting Phase 2 (`repsp` #79) and Phase 3 (`reDMP2`)

1. **Live stop before UMD swap (confirmed)**: Selecting `LINK DISC` → `예` on the Title Screen stops **before** reaching the UMD swap at `sceUtilitySavedata` extended **`mode 12`** (`0x088BD074`): `Reason: Unimplemented sceUtilitySavedata mode 12` (exit code `11`, vblank `1419`).
2. **Additional unimplemented HLE required for Link Disc (confirmed)**:
   * Six `sceUtilitySavedata` extended modes: `mode 12` (`0x088BD074`), `mode 13` (`0x088BD49C`), `mode 15` (`0x088BD7A8`), `mode 17` (`0x088BD688`), `mode 19` (`0x088BD8CC`), `mode 22` (`0x088BD3B0`). `repsp` currently implements `mode 0..8` only.
   * `IoFileMgrForUser::sceIoDevctl` (`0x54F5FB11`, stub `0x088C03E0`): `ms0:` free-space query (command `0x02425818`, `0x0886E1D8`).
   * `sceSuspendForUser::sceKernelVolatileMemTryLock` (`0xA14F40B2`, stub `0x088C09F0`) and `sceKernelVolatileMemUnlock` (`0xA569E425`, stub `0x088C09F8`): copies `linkdisc_{kr,en,jp}.mp` into volatile memory (`0x0886E14C`).
   * `sceUmdUser`: `sceUmdCheckMedium` (`0x46EBB729`, `0x088C0A08`), `sceUmdReplacePermit` (`0xCBE9F02A`, `0x088C0A40`), `sceUmdReplaceProhibit` (`0x87533940`, `0x088C0A28`).
3. **The swapped DMP1 disc is read beyond `PARAM.SFO` — `all.mp` and `all2.mp` are mounted (confirmed)**: Contrary to the earlier *inferred* hypothesis in Design §2 ("only `PARAM.SFO` is read"), immediately after verifying `ULKS46050`, DMP2 calls `0x0886EB94` → `0x08815100` to open `all.mp` and `all2.mp` on the inserted DMP1 UMD. Both `all.mp` and `all2.mp` exist in `roms/ULKS46050/PSP_GAME/USRDIR/`.
4. **DMP2 explicitly closes open archive handles before permitting UMD swap (confirmed)**: `0x0886EC38` (in `CLinkDiscScene::Init`) and `0x0886ECA8` (on returning to DMP2) close `all.mp` and `all2.mp` via `0x08815164` before the swap and reopen them afterward.
5. **`ms0:/PSP/SAVEDATA` and `sceUtility` save root currently point to different directories (confirmed)**: In `src/app/main.cpp`, `ms0:` mounts `<user_data>/ms0` while `sceUtility` uses `--save-dir` (default `<user_data>/savedata`). Link Disc writes `ULKS46116DATA0/linkdisc_game.mp` via `sceUtilitySavedata` and later reads it directly via `ms0:/PSP/SAVEDATA/ULKS46116DATA0/linkdisc_game.mp`, so the two paths must be unified.

---

## 1. Link Disc Menu Location, Prompts, and Button Order

* **Status**: Entry and confirmation dialog **confirmed** (live BMP dumps and generated code). Swap prompt text itself is **unresolved** (to confirm after the swap model and prerequisite HLE).
* **Title Screen carousel** (`0x0884EFA0` loads 6 backgrounds, confirmed):
  `PORTABLE 2` (`title_bg_p2.vce`) → `NETWORK BATTLE` (`title_bg_network.vce`) → `COLLECTION` (`title_bg_collection.vce`) → **`LINK DISC`** (`title_bg_linkdisc.vce`, purple background with green circular-arrows logo) → `MV edition` (`title_bg_mvedition.vce`) → `O.S.T` (`title_bg_ost.vce`) → wraps to `PORTABLE 2`.
* **Button sequence** (verified live on `build/win64-release-ffmpeg`):
  1. From initial `PORTABLE 2`, press `RIGHT` 3 times → selects `LINK DISC`.
  2. Press `START` or `CIRCLE` → confirmation dialog:
     * Text: `"본 모드는 전작인 [ 디제이맥스 포터블 ] 디스크가 필요합니다. 계속 진행하시겠습니까?"`
     * Choices: `예` / `아니오` (default `예`).
  3. Press `CIRCLE` on `예` → creates `CLinkDiscScene` (`0x0886ED7C`, factory `0x08852404`) and runs `Init` (`0x0886D89C`).
  4. **Current live stop**: Immediately in `Init`, `0x088BCE58` → `0x088BD074` calls `sceUtilitySavedataInitStart(mode=12)` → `Reason: Unimplemented sceUtilitySavedata mode 12` (exit code `11`, vblank `1419`).
* **Post-swap screen flow** (statically confirmed from generated code; live verification **to confirm after the swap model**):
  * Message IDs (`0x088331D0`, `this+11792`): `6` (insert previous-title disc), `9` (wrong disc — not DMP1/DMPI), `10` (confirm exit Link Disc), `11` (reinsert DMP2 disc), `12` (wrong disc — not DMP2).
  * After DMP1 is verified, states `7`→`8`→`9` play `system/title/title_portable1_intro.vce` → `title_portable1_wait.vce` with `sound/mode/title.at3`. In state `9` (`0x0886E608`), `START`/`CIRCLE` enters scene `12` (mode/song select **while DMP1 remains inserted**), while `CROSS` prompts msg `10` and transitions to state `6` (wait for DMP2 reinsertion).

---

## 2. UMD Call Sequence, State Machine, and Expected Values

* **Status**: Call order and bit checks **confirmed** statically; live swap behavior **to confirm after the swap model**.

### 2.1 Relevant Stubs and `repsp` Status

| Module | Function | NID | DMP2 Stub | `repsp` Status |
|---|---|---|---:|---|
| `sceUmdUser` | `sceUmdCheckMedium` | `0x46EBB729` | `0x088C0A08` | **Unimplemented** |
| `sceUmdUser` | `sceUmdWaitDriveStatWithTimer` | `0x56202973` | `0x088C0A10` | Implemented (static) |
| `sceUmdUser` | `sceUmdCancelWaitDriveStat` | `0x6AF9B50A` | `0x088C0A18` | Implemented (static) |
| `sceUmdUser` | `sceUmdGetDriveStat` | `0x6B4A146C` | `0x088C0A20` | Implemented (static) |
| `sceUmdUser` | `sceUmdReplaceProhibit` | `0x87533940` | `0x088C0A28` | **Unimplemented** |
| `sceUmdUser` | `sceUmdWaitDriveStat` | `0x8EF08FCE` | `0x088C0A30` | Implemented (static) |
| `sceUmdUser` | `sceUmdActivate` | `0xC6183D47` | `0x088C0A38` | Implemented (static) |
| `sceUmdUser` | `sceUmdReplacePermit` | `0xCBE9F02A` | `0x088C0A40` | **Unimplemented** |
| `sceUmdUser` | `sceUmdDeactivate` | `0xE83742BA` | `0x088C0A48` | Implemented (static) |
| `sceImpose` | `sceImposeSetUMDPopup` | `0x72189C48` | `0x088C0AE0` | Implemented |
| `IoFileMgrForUser` | `sceIoDevctl` | `0x54F5FB11` | `0x088C03E0` | **Unimplemented** |
| `sceSuspendForUser` | `sceKernelVolatileMemTryLock` | `0xA14F40B2` | `0x088C09F0` | **Unimplemented** |
| `sceSuspendForUser` | `sceKernelVolatileMemUnlock` | `0xA569E425` | `0x088C09F8` | **Unimplemented** |
| `sceUtility` | `sceUtilitySavedataInitStart` (`mode 12,13,15,17,19,22`) | `0x50C4CD57` | `0x088C0A68` | Only `mode 0..8` implemented |

### 2.2 `CLinkDiscScene` State Machine (`0x0886D994`, Jump Table `0x088C5A18`)

```mermaid
stateDiagram-v2
    [*] --> Init : CLinkDiscScene::Init (0x0886D89C)
    Init --> State1 : Close all.mp/all2.mp (0x0886EC38)\nQuery savedata (mode 12)
    State1 --> State2 : linkdisc files missing
    State1 --> State4 : linkdisc files present
    State2 --> State3 : Check ms0 3MiB free (sceIoDevctl)\nWrite linkdisc_{kr,en,jp}.mp
    State3 --> State4 : Check ms0 900KiB free (sceIoDevctl)\nWrite linkdisc_game.mp
    State4 --> State5 : Copy to volatile mem (0x0886E14C)\nsceImposeSetUMDPopup(0)\nsceUmdReplacePermit() + msg 6
    State5 --> State5 : Poll: !(GetDriveStat & 0x20)\nAfter 15s: CheckMedium()==0 -> msg 6
    State5 --> State5 : GetDriveStat & 0x20\nPARAM.SFO != DMP1/DMPI -> msg 9
    State5 --> State7 : PARAM.SFO == ULKS46050 / ULKS46059\nReplaceProhibit + Popup(1)\nLoad discstock + open DMP1 all.mp/all2.mp
    State7 --> State8 : title_portable1_intro.vce
    State8 --> State9 : title_portable1_wait.vce
    State9 --> Scene12 : START/CIRCLE (play with DMP1 inserted)
    State9 --> State6 : CROSS + confirm msg 10\nPopup(0) + ReplacePermit() + msg 11
    State6 --> State6 : Poll: !(GetDriveStat & 0x20)\nAfter 15s: CheckMedium()==0 -> msg 12
    State6 --> State6 : GetDriveStat & 0x20\nPARAM.SFO != ULKS46116 -> msg 12
    State6 --> State10 : PARAM.SFO == ULKS46116\nReplaceProhibit + Popup(1)\nClose DMP1 + reopen DMP2 all.mp/all2.mp
    State10 --> [*] : Return to Title (scene 24)
```

### 2.3 Detailed Call Rules and Expected Values

* **When `ReplacePermit` / `ReplaceProhibit` are called (confirmed)**:
  * **Permit**: Immediately before prompting for DMP1 (`0x0886E9B8`: `sceImposeSetUMDPopup(0)` → `sceUmdReplacePermit()`) and before prompting for DMP2 reinsertion (`0x0886EA14`: `sceImposeSetUMDPopup(0)` → `sceUmdReplacePermit()`). Retrying after a wrong disc (`0x0886E840`) calls them again.
  * **Prohibit**: Immediately after `0x0886EA94` confirms the expected disc (`0x0886EA70`: `sceUmdReplaceProhibit()` → `sceImposeSetUMDPopup(1)`).
* **`sceUmdGetDriveStat` bits checked (confirmed)**:
  * Both state `5` (`0x0886DCE0`) and state `6` (`0x0886DE40`) test only `andi $v0, $v0, 0x20` (**`PSP_UMD_READY` = `0x20`**).
  * No other bits (`PSP_UMD_NOT_PRESENT`, `PSP_UMD_PRESENT`, `PSP_UMD_CHANGED`) are tested directly here; the swap model must clear `0x20` while ejected/switching and set `0x20` once the new disc is ready.
* **`sceUmdCheckMedium` return semantics (confirmed)**:
  * Called in state `5` (`0x0886DE0C`) and state `6` (`0x0886DEC8`) only when `!(GetDriveStat() & 0x20)` has persisted for **>= 15,000 ms** (`sltiu $v0, $v0, 15000`).
  * Return **`0`** = no disc present (re-displays prompt msg `6` or `12`). **Nonzero** = disc present / becoming ready (continues polling).
* **Wait mechanism (confirmed)**:
  * `CLinkDiscScene` uses **per-frame polling** of `sceUmdGetDriveStat()`, not `WaitDriveStat*`.
  * A separate background I/O retry layer (`0x0882E484..0x0882E5C0`) calls `sceUmdWaitDriveStatWithTimer(0x20, 10000000)` (10 s timeout) or `sceUmdWaitDriveStat(0x20)` if `!(GetDriveStat() & 0x20)`.
* **Open-file handling (confirmed)**:
  * `0x0886EC38` (in `Init`) closes DMP2's `all.mp` and `all2.mp` (`0x08815164`) before permitting replacement.
  * `0x0886ECA8` (on return to DMP2) closes DMP1's `all.mp` and `all2.mp` (`0x08815164`) and reopens DMP2's (`0x0881506C`).

---

## 3. Files Read from the Swapped Disc (DMP1)

* **Status**: Statically **confirmed**; live I/O **to confirm after the swap model**.

1. **`disc0:/PSP_GAME/PARAM.SFO`** (`0x0886EA94`):
   * Reads 16 bytes at offset `296` to identify the disc.
   * Verified on local dumps:
     * `roms/ULKS46116/PSP_GAME/PARAM.SFO` (472 B) offset 296 = `ULKS46116\0...` → returns `2`
     * `roms/ULKS46050/PSP_GAME/PARAM.SFO` (472 B) offset 296 = `ULKS46050\0...` → returns `0`
     * `ULKS46059` → returns `1`
     * anything else → returns `3`
2. **`disc0:/PSP_GAME/USRDIR/all.mp` and `disc0:/PSP_GAME/USRDIR/all2.mp`** (`0x0886EB94` → `0x08815100`):
   * `0x0886EB94` closes prior handles (`0x08815164`) and calls `0x08815100`, which opens `"all.mp"` (`0x088C7BEC`) and `"all2.mp"` (`0x088C7BF4`) on the inserted DMP1 UMD.
   * Both files exist in `roms/ULKS46050/PSP_GAME/USRDIR/` (**confirmed**).
   * During Link Disc gameplay, DMP1 songs/charts/audio/BGA are streamed from DMP1's `all.mp`/`all2.mp` while DMP2 engine/UI assets come from the cached `linkdisc_*.mp` files on the Memory Stick and in volatile memory (**inferred**, to confirm after the swap model).

---

## 4. `linkdisc_*.mp` Creation, Sizes, Sources, and Save (`SD.BIN`)

* **Status**: Extraction timing, sizes, and archive sources **confirmed**; `SD.BIN` flags **unresolved** (to confirm after the swap model).

* **When written**: Not after returning to DMP2, but **before the UMD swap** (states `1`–`3`, while DMP2 is still inserted) into `ULKS46116DATA0` via `sceUtilitySavedata` (**confirmed**).
  * State `1` (`0x0886D9F0`): queries existing files via `0x088BD074` (`sceUtilitySavedata mode 12`). If present, skips directly to state `4`.
  * State `2` (`0x0886DA70`): checks `0x00300000` (3 MiB) free on `ms0:` via `sceIoDevctl(0x02425818)` and writes the language-specific `linkdisc_{kr,en,jp}.mp`.
  * State `3` (`0x0886DB58`): checks `0x000E1000` (900 KiB) free on `ms0:` and writes `linkdisc_game.mp`.
* **Archive sources and sizes** (`0x088BE3E8` / `BOOT.BIN` strings, confirmed):

  | DMP2 Archive Entry | Size (hex) | Size (bytes) | Role |
  |---|---:|---:|---|
  | `system/linkdisc_kr.mp` | `0x0022ABAD` | 2,272,173 | Korean UI/resources (copied to volatile memory in state 4) |
  | `system/linkdisc_en.mp` | `0x0023277A` | 2,303,866 | English UI/resources |
  | `system/linkdisc_jp.mp` | `0x00233B79` | 2,308,985 | Japanese UI/resources |
  | `system/linkdisc_game.mp` | `0x00076404` | 484,356 | Opened directly by play scene (`0x08853628`, `0x0885462C`, `0x0885479C`) at `ms0:/PSP/SAVEDATA/ULKS46116DATA0/linkdisc_game.mp` |

* **`sceUtilitySavedata` extended modes used** (`0x088BD074`–`0x088BD8CC` confirmed; PSPSDK enum names **inferred**):
  * `0x088BD074`: `mode = 12` (`PSP_UTILITY_SAVEDATA_TYPE_FILES`)
  * `0x088BD49C`: `mode = 13` (`MAKEDATASECURE`)
  * `0x088BD7A8`: `mode = 15` (`READDATASECURE`)
  * `0x088BD688`: `mode = 17` (`WRITEDATASECURE`)
  * `0x088BD8CC`: `mode = 19` (`ERASESECURE`)
  * `0x088BD3B0`: `mode = 22` (`GETSIZE`)
* **After switching back to DMP2 and `SD.BIN` persistence**:
  * When `ULKS46116` is confirmed in state `6`, `0x0886E0A4` cleans up Link Disc resources, calls `0x0886ECA8` to close DMP1's archives and reopen DMP2's `all.mp`/`all2.mp`, and returns to Title (scene `24`) (**confirmed**).
  * `0x0886E0A4` does not directly write `SD.BIN`; which fields of `SD.BIN` record Link Disc progress or unlocks is **unresolved** (**to confirm after the swap model**).

---

## 5. What Gets Unlocked (`discstock_dmp1.csv`)

* **Status**: Call path **confirmed**; exact song/mode list **unresolved** (to confirm after the swap model).
* In state `5`, right after `0x0886EA70` and **before** `0x0886EB94` (DMP1 archive mount), `ULKS46050` calls `0x0886E7BC(this, 0)` (`discstock_dmp1.csv`), and `ULKS46059` calls `0x0886E7BC(this, 1)` (`discstock_dmpi.csv`).
* `0x0886E7BC` calls `0x088295B4`, which zeroes global table `0x08920BBC` (`3972` bytes) and populates it from the CSV (**confirmed**).
* Because `discstock_dmp1.csv` is inside the encrypted `MP02` archive, its exact song/mode entries and whether unlocks persist after returning to DMP2 are **to confirm after the swap model**.

---

## 6. Direct `ms0:/PSP/SAVEDATA/...` Access vs. `sceUtility` Save Root (`--save-dir`)

* **Status**: **Confirmed** (from `src/app/main.cpp`).
* Current mismatch:
  * `ms0:` VFS mount (`src/app/main.cpp` lines 644, 698): fixed to `repsp::platform::GetUserDataDirectory("redmp2") / "ms0"`, ignoring `--save-dir`.
  * `sceUtility` `save_root` (`src/app/main.cpp` lines 841–843): uses `--save-dir` if given, else `GetUserDataDirectory("redmp2") / "savedata"`.
* **Impact**:
  * States `2`–`3` write `<save_root>/ULKS46116DATA0/linkdisc_game.mp` through `sceUtilitySavedata`.
  * The play scene (`0x08853628`, `0x0885462C`, `0x0885479C`) opens `ms0:/PSP/SAVEDATA/ULKS46116DATA0/linkdisc_game.mp` directly via `IoFileMgrForUser` (format string `"/PSP/SAVEDATA/%sDATA0/linkdisc_game.mp"` at `0x088CB3DA`).
  * Because the two directories differ, direct reads cannot find `linkdisc_game.mp` today.
* **Direction for Phase 3**:
  * Unify `ms0:/PSP/SAVEDATA` and `sceUtility` `save_root` to the same directory (and cover `--save-dir` plus migration of any existing `<user_data>/savedata` saves in the Phase 3 design).
