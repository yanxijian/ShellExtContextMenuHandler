# ShellExtContextMenuHandler

**한국어** · [English](README.md) · [简体中文](README.zh-CN.md) · [日本語](README.ja.md)

Microsoft의 C++ 셸 확장 샘플을 출발점으로 한, 설정 가능한 Windows 탐색기 상황에 맞는 메뉴(컨텍스트 메뉴) 확장입니다. 메뉴 항목, Shell 등록 대상, 필터, 게이트, 실행기, 아이콘을 설정으로 바꾸고, 복잡한 동작은 C++에 둡니다.

## 면책

이 프로젝트는 주로 AI 에이전트의 도움을 받아 개발했습니다. 코드, 설계, 문서의 일부는 충분하고 엄격하며 포괄적인 검증을 거치지 않았습니다. 학습, 조사, 기술 참고용이며 모든 시스템, 환경, 용도에서 정확성, 안정성, 보안을 보장하지 않습니다.

중요한 프로젝트, 상용 소프트웨어, 운영 환경에 쓰기 전에 적절한 코드 리뷰, 맞춤화, 보안 평가, 호환성 검증을 하세요. 이 프로젝트의 사용으로 인한 데이터 손실, 시스템 장애, 업무 중단, 그 밖의 직접·간접 손실에 대해 작성자는 책임지지 않습니다.

## 기능

- 기본으로 다섯 가지 Shell 대상을 등록합니다: 파일, 디렉터리, 디렉터리 배경, 드라이브, 파일 시스템 객체.
- `config/menu.json`에서 메뉴 항목, 대상 필터, 선택 필터, 게이트/실행기 체인, 동작을 설정합니다.
- `config/registration.json`에서 COM/Shell 등록 대상을 설정합니다.
- 설정 전체를 엄격히 검증합니다. 필드 타입 오류, 알 수 없는 게이트/실행기, 알 수 없는 `actionType`, 중복 id는 필터를 조용히 넓히는 대신 내장 메뉴로 되돌립니다.
- 레지스트리를 건드리기 전에 `registration.json`을 검증하고, 잘못된 파일이면 기존 설치를 바꾸지 않습니다. 등록/해제가 성공하면 Shell에 알립니다.
- `messageBox`와 `launch` 동작에 더해 사용자 정의 실행기를 추가할 수 있습니다.
- SVG/BMP 아이콘을 지원합니다. SVG는 기존 NanoSVG 프로바이더가 읽어 래스터화하며 DPI 배율을 따릅니다.
- 게이트는 세 단계: Extension, Item, Presentation.
- 내장 게이트: `extensionPass`, `jsonFilter`, `presentationPass`, `demo:hideTemp`, `demo:readOnlyDisable`. 내장 실행기: `messageBox`, `launch`, `demo:actionLog`.
- 빌드 산출물에 더블클릭으로 쓸 수 있는 `register.bat`과 `unregister.bat`이 들어 있습니다(UAC 승격 포함). DLL 비트수에 맞춰 `System32` 또는 `SysWOW64`의 `regsvr32`를 고릅니다.
- `[ShellExt]` 접두사를 붙인 `OutputDebugString` 디버그 로그.
- CTest 단위 테스트를 함께 제공하며, 설정 파싱, 자리표시자 확장, 컨텍스트 구성, 실행 명령 해석, 등록 설정 파싱, COM 예외 경계를 확인합니다. CI가 자동으로 돌립니다.

## 빠른 시작

### 요구 사항

- Windows x64
- Visual Studio 2026 C++ 툴체인
- CMake 3.20 이상
- 설정 검증용 Python 3.9 이상

설계 대상은 Windows 7 SP1 이상입니다. 최종 DLL은 지원하려는 Windows 버전과 탐색기 비트수에서 검증하세요.

### 빌드

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DSHELLEXT_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
python tools/validate_menu_json.py
```

단위 테스트는 기본적으로 꺼져 있습니다(`SHELLEXT_BUILD_TESTS=ON`으로 켬). `tests/`에 있고, 복사본이 아니라 실제 소스를 컴파일합니다.

DLL은 MSVC CRT를 정적으로 링크하므로(`/MT`) 대상 컴퓨터에 VC++ 런타임이 따로 필요 없고, 그만큼 DLL이 조금 커집니다.

`tools/generate-vs.ps1`로 Visual Studio 프로젝트를 생성할 수도 있습니다.

출력은 `build/bin/Release/`에 있습니다:

```text
CppShellExtContextMenuHandler.dll
menu.json
registration.json
register.bat
unregister.bat
icons/
```

### 등록, 확인, 해제

DLL 출력 디렉터리에서 `register.bat`을 더블클릭합니다. 배치 파일 옆의 DLL을 쓰고, `regsvr32`를 호출하기 전에 승격을 요청합니다. `unregister.bat`을 더블클릭하면 확장이 제거됩니다.

관리자 권한 PowerShell을 쓸 수도 있습니다:

```powershell
.\tools\register.ps1 -Action register
.\tools\register.ps1 -Action unregister
```

`register.ps1`은 JSON 문법과 등록 대상을 검증하고(Python이 있으면 전체 검증도 실행), `registration.json`을 복사한 뒤 `regsvr32`를 호출하며, 등록이 성공한 뒤에만 `menu.json`과 `config/icons/`를 복사합니다. 등록이 실패하면 이전 `registration.json`으로 되돌립니다. DLL이나 설정을 바꾸면 해제했다가 다시 등록하세요. 성공 시 `SHChangeNotify`를 보내므로 보통 탐색기를 다시 시작할 필요가 없습니다. `registration.json`이 잘못되면 기존 레지스트리를 바꾸지 않고 등록이 실패합니다.

`register.bat`/`unregister.bat`과 `register.ps1`은 DLL의 PE 머신 타입을 보고 맞는 `regsvr32`를 고릅니다(x64는 `System32`, x86은 `SysWOW64`).

### 수동 스모크 테스트

1. `unregister.bat`을 실행한 뒤 `register.bat`을 실행한다.
2. 일반 파일을 오른쪽 클릭해 `Demo -F`가 나오는지 확인한다.
3. 디렉터리를 오른쪽 클릭해 `Demo -D`가 나오는지 확인한다.
4. 디렉터리의 빈 곳을 오른쪽 클릭해 `Demo -DB`가 나오는지 확인한다.
5. 드라이브 루트를 오른쪽 클릭해 `Demo -DR`이 나오는지 확인한다.
6. 파일과 디렉터리를 함께 선택해 `Demo -FS`가 나오는지 확인한다.
7. 항목을 실행해 메시지 상자에 대상 이름이 나오는지 확인한다.

## 저장소 규칙과 검사

기계적으로 판정할 수 있는 규칙은 [RepoRules](https://github.com/yanxijian/RepoRules)에서 가져오고(검사기는 `scripts/repo_rules.py`로 내장, `repo-rules.json`으로 설정), CI에서는 `.github/workflows/hygiene.yml`이 실행합니다:

- C/C++는 UTF-8 BOM + CRLF, Markdown은 CRLF, CMake·스크립트·JSON·YAML은 LF(`.gitattributes` / `.editorconfig` 참조).
- C/C++ 줄은 140열 이내. 헤더는 `#pragma once`로 시작. `#pragma comment(lib)` 금지 — Windows 라이브러리는 CMake로 연결한다.
- `CMakeLists.txt`, `src/resources/Version.rc`, `include/shell_ext/common.h`의 버전이 일치해야 한다. [CHANGELOG.md](CHANGELOG.md)에 `[Unreleased]` 절을 유지한다.
- Markdown 상대 링크와 중국어/영어 문서 쌍이 해석되어야 한다.

```powershell
python scripts/repo_rules.py check --config repo-rules.json   # 검사
python scripts/repo_rules.py fix   --config repo-rules.json   # BOM/줄바꿈 복구
python -m unittest discover -s scripts/tests -v
```

원하면 `git config core.hooksPath .githooks`로 pre-commit 훅을 넣을 수 있습니다. 규칙과 리뷰 우선순위는 [AGENTS.md](AGENTS.md), 진행 방식은 [CONTRIBUTING.en.md](CONTRIBUTING.en.md), 비공개 제보는 [SECURITY.en.md](SECURITY.en.md)를 보세요.

## 설정

### Shell 등록 대상

`config/registration.json`:

```json
{
  "schemaVersion": 1,
  "shellRegistrations": [
    "file",
    "directory",
    "directoryBackground",
    "drive",
    "fileSystemObject"
  ]
}
```

| 대상 | 레지스트리 위치 | 실행 시 문맥 |
|--------|------------------|-----------------|
| `file` | `*` | 파일만 선택 |
| `directory` | `Directory` | 디렉터리만 선택 |
| `directoryBackground` | `Directory\\Background` | 디렉터리에서 아무것도 선택하지 않음 |
| `drive` | `Drive` | 드라이브 루트 |
| `fileSystemObject` | `AllFilesystemObjects` | 파일과 디렉터리를 함께 선택 |

`registration.json`이 없으면 등록은 `file`만 됩니다. 파일은 있는데 스키마가 잘못됐거나, 알 수 없는 대상, 중복 대상, 배열이 아니면 레지스트리를 조용히 바꾸지 않고 등록이 실패합니다. 해제는 알려진 대상을 모두 제거하므로 예전 등록도 정리됩니다.

### `menu.json`의 루트 필드

```json
{
  "schemaVersion": 1,
  "extensionGates": ["extensionPass"],
  "itemGates": ["jsonFilter"],
  "presentationGates": ["presentationPass"],
  "executors": ["messageBox"],
  "menuItems": []
}
```

| 필드 | 설명 |
|-------|-------------|
| `schemaVersion` | 선택. 지금은 `1`만 받으므로 나중에 형식이 바뀌면 호환되지 않는 파일을 거부할 수 있다 |
| `extensionGates` | 1단계 체인. 확장이 참여할지 결정 |
| `itemGates` | 2단계 체인. 항목이 후보가 될지 결정. `gates`는 별칭 |
| `presentationGates` | 3단계 체인. 숨김/비활성/활성 중 하나를 반환 |
| `executors` | 기본 동작 실행기 체인 |
| `menuItems` | 메뉴 항목 정의 |

`menu.json`은 UTF-8(BOM 허용)이며 `third_party/nlohmann/json`으로 파싱합니다. 필드 타입 오류, 알 수 없는 게이트/실행기, 알 수 없는 `actionType`, 중복 id 같은 문서 수준 오류는 내장 메뉴로 되돌립니다. 알 수 없는 체인 이름을 조용히 무시하지는 않습니다.

체인 덮어쓰기 규칙: 항목별로 명시한 체인은 루트 기본값에 **덧붙이지 않고 완전히 대체**합니다. 루트 기본값은 체인이 없거나 비었을 때만 적용됩니다.

### 항목별 필드

| 필드 | 설명 |
|-------|-------------|
| `id` | 고유 식별자 |
| `label` | 메뉴 문자열. `&`는 액셀러레이터 |
| `verb` | 문자열 명령 이름. InvokeCommand가 이것 또는 `canonicalName`으로 매칭한다 |
| `helpText` | 탐색기 상태 표시줄 설명 |
| `canonicalName` | 정규 명령 이름. 매칭에서 `verb`와 같은 뜻으로 쓰인다 |
| `icon` | DLL 디렉터리 기준 상대 경로의 SVG/BMP |
| `targets` | `file`, `directory`, `directoryBackground`, `drive`, `fileSystemObject` |
| `separatorAfter` | 항목 뒤에 구분선을 넣는다 |
| `extensions` | `[".cpp", ".h"]` 같은 허용 확장자 |
| `excludeExtensions` | 거부할 확장자 |
| `minSelection` / `maxSelection` | 선택 개수. `maxSelection: 0`은 무제한 |
| `filesOnly` / `foldersOnly` | 선택된 객체 종류를 제한 |
| `actionType` | `messageBox` 또는 `launch` |
| `actionTitle` / `actionTemplate` | 메시지 상자의 제목과 내용 |
| `actionCommand` / `actionShowWindow` | 실행 명령과 창 옵션 |
| `extensionGates` / `itemGates` / `presentationGates` / `executors` | 항목별 체인 덮어쓰기. 명시한 체인은 루트 기본값을 대체한다 |

`targets`가 빈 배열이면 기존의 일반 선택 필터와의 호환을 유지합니다. 새 항목은 `targets`를 명시하세요.

### 자리표시자

| 토큰 | 의미 |
|-------|---------|
| `%1` | 처음 선택한 대상의 전체 경로. 없으면 현재 디렉터리 |
| `%*` | 선택한 모든 경로를 따옴표로 감싸 이어 붙인 것 |
| `%D` | 처음 선택한 대상의 상위 디렉터리. 없으면 현재 디렉터리 |
| `%N` | 처음 선택한 대상(또는 현재 디렉터리)의 이름. 상위 경로는 제외 |

`menu.json`이 없거나 잘못되면 확장은 `include/shell_ext/common.h`의 내장 항목과 기본 게이트 체인으로 돌아갑니다.

자리표시자 확장은 한 번만 합니다. 치환된 문자열을 다시 훑지 않으므로 `%1`이나 `%N`이 들어간 파일 이름을 선택해도 안전합니다.

### 실행 명령 해석

`launch` 동작은 실행 파일을 `actionCommand`에서 명시적으로 해석한 뒤 시작합니다:

- 따옴표로 감싼 경로는 그대로 쓴다.
- 따옴표가 없고 공백이 있는 경로는 CreateProcess 문서에 적힌 접두사 점진 탐색을 따른다.
- 명령 이름만 있는 경우(`notepad.exe` 등)는 시스템 디렉터리와 PATH의 절대 경로에서만 해석한다. **현재 디렉터리는 절대 탐색하지 않으므로** 같은 이름의 실행 파일에 가로채이지 않는다.
- 해석할 수 없는 명령은 실행하지 않고 로그를 남긴 뒤 거부한다.

### 긴 경로

선택한 대상의 경로는 `MAX_PATH`에 묶이지 않습니다. `%1`, `%*`, `%N`, `%D` 모두 동적 `std::wstring`으로 다룹니다. 디렉터리 배경 경로는 먼저 `SHGetPathFromIDListEx`를 시도하고 안 되면 `SHGetPathFromIDListW`로 넘어갑니다. 파일 특성 조회는 일반 경로에서 실패하면 `\\?\` 확장 경로로 다시 시도합니다.

`MAX_PATH`를 넘는 실제 경로에는 시스템의 긴 경로 정책과, 긴 경로를 다루는 탐색기가 필요합니다. 그렇지 않으면 Shell이 그런 경로를 확장에 넘기지 않습니다.

## 프로젝트 확장

### 메뉴 항목 추가

1. `config/menu.json`의 `menuItems`에 객체를 넣는다.
2. 고유한 `id`, `verb`, `targets`, 필터, 동작 필드를 설정한다.
3. SVG/BMP 자산을 `config/icons/`에 두고 상대 경로로 참조한다.
4. `python tools/validate_menu_json.py`를 실행한다.

### 게이트 추가

1. `src/gates/`에 `IExtensionGate`, `IMenuItemGate`, `IMenuItemPresentationGate` 구현을 넣는다.
2. `GateRegistry::RegisterBuiltInGates`에 이름을 등록한다.
3. `menu.json`의 알맞은 체인에서 그 이름을 쓴다.

Extension 게이트는 확장이 참여할지, Item 게이트는 후보에 들지, Presentation 게이트는 `Hidden`, `Disabled`, `Enabled` 중 무엇을 반환할지 결정합니다.

### 실행기 추가

1. `src/actions/`에 `IActionExecutor` 구현을 넣는다.
2. `ExecutorRegistry::RegisterBuiltInExecutors`에 등록한다.
3. 루트 또는 항목별 `executors` 배열에 이름을 넣는다.
4. 새 설정 필드가 필요하면 `MenuAction`과 `MenuConfig`를 확장한다.

### Shell 대상 추가

1. `ShellTargetType`에 열거값을 넣는다.
2. `ParseShellTargetType`과 `GetShellTargetRegistryFileType`을 고친다.
3. `ContextBuilder`에 실행 시 판정을 넣는다.
4. 대상 매칭과 등록/해제의 알려진 목록을 갱신한다.
5. `tools/validate_menu_json.py`, README, 아키텍처 문서를 갱신한다.

### COM 등록 변경

등록 진입점은 `src/extension/dllmain.cpp`, 레지스트리 도우미는 `src/registry/Reg.cpp`에 있습니다. 부분 등록 실패 시 롤백, 해제의 멱등성, 예전 대상 정리를 지키세요. 지워도 되는 것은 이 프로젝트의 CLSID 하위 키뿐이며, `ContextMenuHandlers` 상위 키를 통째로 지우면 안 됩니다.

## 디버깅과 문제 해결

- DebugView로 `[ShellExt]` 로그를 본다.
- DLL 비트수를 탐색기에 맞춘다.
- DLL이나 설정을 바꾸면 다시 등록한다. 필요하면 탐색기를 다시 시작한다.
- 메뉴가 안 보이면 등록 대상, 항목의 `targets`, `filesOnly`/`foldersOnly`, 게이트 체인을 함께 확인한다.
- 등록이 실패하면 승격, `registration.json`, `regsvr32` 비트수를 확인한다.

## 프로젝트 구조

```text
config/                 실행 시 메뉴, 등록 대상, 아이콘
src/extension/          COM DLL 진입점, ClassFactory, FileContextMenuExt, ComGuard
src/context/            선택 문맥과 Shell 대상 판정
src/gates/              게이트 인터페이스, 레지스트리, 내장 게이트
src/actions/            실행기 인터페이스, 레지스트리, 동작
src/icons/              SVG/BMP 아이콘 프로바이더
src/menu/               설정, 필터링, 삽입, 호출
src/registry/           COM/Shell 레지스트리 도우미
tools/                  빌드 도우미, 등록 스크립트, 검증
scripts/                저장소 규칙 검사기와 그 테스트
tests/                  CTest 단위 테스트
third_party/            서드파티(nlohmann/json, NanoSVG)
docs/                   아키텍처와 검증 문서
```

## 참고

- 64비트 탐색기에는 64비트 DLL이 필요합니다. 32비트 탐색기에서는 맞는 비트수를 쓰세요. 등록 스크립트가 `System32`/`SysWOW64`의 `regsvr32`를 자동으로 고릅니다.
- COM 등록에는 관리자 권한이 필요합니다.
- Windows 7 지원은 설계 목표입니다. 릴리스 전에 Win7 SP1 x64 회귀를 마치세요. 현재 VS 2026 툴체인과 정적 CRT는 Win7에서 별도 검증이 필요합니다. 지원되지 않으면 지원되는 예전 툴체인으로 다시 빌드하세요.
- DLL은 MSVC CRT를 정적으로 링크합니다. 새 의존성을 넣으면 런타임 요구 사항을 다시 검토하세요.
- `build/`, `build-*`, 서드파티 디렉터리의 생성물은 편집하지 마세요.

## COM 식별자와 라이선스

포크를 배포할 때는 `src/extension/dllmain.cpp`의 `CLSID_FileContextMenuExt`와 `include/shell_ext/common.h`의 표시 이름을 바꾸세요.

이 프로젝트는 Microsoft의 샘플을 출발점으로 합니다. 소스 파일은 원래 Microsoft 헤더를 유지하며 [Ms-PL](https://www.microsoft.com/opensource/licenses.mspx#Ms-PL)을 따릅니다. 라이선스 전문은 `LICENSE`에, nlohmann/json과 NanoSVG 고지는 `THIRD_PARTY_NOTICES.md`에 있습니다.
