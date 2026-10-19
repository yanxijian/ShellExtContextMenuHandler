# ShellExtContextMenuHandler

**日本語** · [English](README.md) · [简体中文](README.zh-CN.md) · [한국어](README.ko.md)

<!-- translated-from: README.md sha256:3db399f4e11f -->

Microsoft の C++ シェル拡張サンプルを出発点にした、設定可能な Windows エクスプローラーのコンテキストメニュー拡張です。メニュー項目、Shell の登録ターゲット、フィルタ、ゲート、実行器、アイコンを設定で変えられ、込み入った挙動は C++ 側に置きます。

## 免責

本プロジェクトは主に AI エージェントの支援を受けて開発しました。コード、設計、ドキュメントの一部は、十分かつ厳密で網羅的な検証を受けていません。学習、調査、技術的な参照のためのものであり、あらゆるシステム、環境、用途での正しさ、安定性、安全性は保証しません。

重要なプロジェクト、商用ソフトウェア、本番環境で使う前に、適切なコードレビュー、カスタマイズ、セキュリティ評価、互換性の検証を行ってください。本プロジェクトの利用に起因するデータの消失、システム障害、業務の中断、その他の直接的・間接的な損害について、作者は責任を負いません。

## 特長

- 既定で 5 種類の Shell ターゲットを登録します：ファイル、ディレクトリ、ディレクトリの背景、ドライブ、ファイルシステムオブジェクト。
- `config/menu.json` でメニュー項目、ターゲットのフィルタ、選択のフィルタ、ゲート／実行器の連鎖、動作を設定します。
- `config/registration.json` で COM/Shell の登録ターゲットを設定します。
- 設定全体を厳密に検証します。項目の型違い、未知のゲート／実行器、未知の `actionType`、重複した id は、フィルタを黙って広げるのではなく組み込みメニューへ戻します。
- レジストリに触る前に `registration.json` を検証し、不正なら既存のインストールを変えません。登録／解除の成功時は Shell に通知します。
- `messageBox` と `launch` の動作に加え、独自の実行器を追加できます。
- SVG/BMP アイコンに対応。SVG は既存の NanoSVG プロバイダが読み込んでラスタライズし、DPI に追従します。
- ゲートは 3 段：Extension、Item、Presentation。
- 組み込みゲート：`extensionPass`、`jsonFilter`、`presentationPass`、`demo:hideTemp`、`demo:readOnlyDisable`。組み込み実行器：`messageBox`、`launch`、`demo:actionLog`。
- ビルド出力に、ダブルクリックで使える `register.bat` と `unregister.bat` を含みます（UAC 昇格付き）。DLL のビット数に合わせて `System32` か `SysWOW64` の `regsvr32` を選びます。
- `[ShellExt]` を頭に付けた `OutputDebugString` によるデバッグログ。
- CTest の単体テストを同梱し、設定の解析、プレースホルダーの展開、コンテキストの組み立て、起動コマンドの解決、登録設定の解析、COM 例外の境界を確認します。CI で自動実行します。

## クイックスタート

### 動作要件

- Windows x64
- Visual Studio 2026 の C++ ツールチェーン
- CMake 3.20 以降
- 設定の検証に Python 3.9 以降

設計上の対象は Windows 7 SP1 以降です。最終的な DLL は、対応する予定の Windows の版とエクスプローラーのビット数で検証してください。

### ビルド

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DSHELLEXT_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
python tools/validate_menu_json.py
```

単体テストは既定で無効です（`SHELLEXT_BUILD_TESTS=ON` で有効）。`tests/` にあり、コピーではなく実際のソースをコンパイルします。

DLL は MSVC の CRT を静的リンクする（`/MT`）ため、配布先に VC++ ランタイムは不要です。そのぶん DLL は少し大きくなります。

`tools/generate-vs.ps1` で Visual Studio のプロジェクトを生成することもできます。

出力は `build/bin/Release/` にあります：

```text
CppShellExtContextMenuHandler.dll
menu.json
registration.json
register.bat
unregister.bat
icons/
```

### 登録・確認・解除

DLL の出力ディレクトリで `register.bat` をダブルクリックします。バッチファイルと同じ場所の DLL を使い、`regsvr32` を呼ぶ前に昇格を求めます。`unregister.bat` をダブルクリックすると拡張を削除します。

管理者権限の PowerShell を使う方法もあります：

```powershell
.\tools\register.ps1 -Action register
.\tools\register.ps1 -Action unregister
```

`register.ps1` は JSON の構文と登録ターゲットを検証し（Python があれば完全な検証も実行）、`registration.json` をコピーして `regsvr32` を呼び、登録が成功したときだけ `menu.json` と `config/icons/` をコピーします。登録に失敗した場合は以前の `registration.json` に戻します。DLL や設定を変えたら、いったん解除してから再登録してください。成功時は `SHChangeNotify` を送るため、通常はエクスプローラーの再起動は不要です。`registration.json` が不正なら、既存のレジストリを変えずに登録が失敗します。

`register.bat`／`unregister.bat` と `register.ps1` は DLL の PE マシンタイプを見て、対応する `regsvr32`（x64 なら `System32`、x86 なら `SysWOW64`）を選びます。

### 手動のスモークテスト

1. `unregister.bat` を実行してから `register.bat` を実行する。
2. 通常のファイルを右クリックし、`Demo -F` が出ることを確認する。
3. ディレクトリを右クリックし、`Demo -D` が出ることを確認する。
4. ディレクトリ内の何もない場所を右クリックし、`Demo -DB` が出ることを確認する。
5. ドライブのルートを右クリックし、`Demo -DR` が出ることを確認する。
6. ファイルとディレクトリを同時に選択し、`Demo -FS` が出ることを確認する。
7. 項目を実行し、メッセージボックスに対象の名前が出ることを確認する。

## リポジトリの規則とチェック

機械的に判定できる規則は [RepoRules](https://github.com/yanxijian/RepoRules) から取り（チェッカーは `scripts/repo_rules.py` に同梱、`repo-rules.json` で設定）、CI では `.github/workflows/hygiene.yml` が実行します：

- C/C++ は UTF-8 BOM + CRLF、Markdown は CRLF、CMake・スクリプト・JSON・YAML は LF（`.gitattributes` / `.editorconfig` を参照）。
- C/C++ の行は 140 桁以内。ヘッダーは `#pragma once` で始める。`#pragma comment(lib)` は禁止で、Windows のライブラリは CMake 経由でリンクする。
- `CMakeLists.txt`、`src/resources/Version.rc`、`include/shell_ext/common.h` のバージョンが一致していること。[CHANGELOG.md](CHANGELOG.md) に `[Unreleased]` 節を保つこと。
- Markdown の相対リンクと、日本語／英語のドキュメント対が解決できること。

```powershell
python scripts/repo_rules.py check --config repo-rules.json   # 検査
python scripts/repo_rules.py fix   --config repo-rules.json   # BOM/改行の修復
python -m unittest discover -s scripts/tests -v
```

任意で `git config core.hooksPath .githooks` により pre-commit フックを入れられます。規則とレビューの優先順位は [AGENTS.md](AGENTS.md)、進め方は [CONTRIBUTING.en.md](CONTRIBUTING.en.md)、非公開での報告は [SECURITY.en.md](SECURITY.en.md) を参照してください。

## 設定

### Shell の登録ターゲット

`config/registration.json`：

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

| ターゲット | レジストリの位置 | 実行時の文脈 |
|--------|------------------|-----------------|
| `file` | `*` | ファイルだけを選択 |
| `directory` | `Directory` | ディレクトリだけを選択 |
| `directoryBackground` | `Directory\\Background` | ディレクトリ内で何も選択していない |
| `drive` | `Drive` | ドライブのルート |
| `fileSystemObject` | `AllFilesystemObjects` | ファイルとディレクトリの混在選択 |

`registration.json` がなければ、登録は `file` だけになります。ファイルはあるがスキーマが不正、未知のターゲット、重複したターゲット、配列でない場合は、レジストリを黙って変えずに登録が失敗します。解除は既知のターゲットをすべて外すので、古い登録も掃除できます。

### `menu.json` のルート項目

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

| 項目 | 説明 |
|-------|-------------|
| `schemaVersion` | 任意。現在は `1` のみ受け付けるので、将来の形式変更で非互換ファイルを弾ける |
| `extensionGates` | 第 1 段の連鎖。拡張が関与するかを決める |
| `itemGates` | 第 2 段の連鎖。項目が候補になるかを決める。`gates` は別名 |
| `presentationGates` | 第 3 段の連鎖。非表示・無効・有効を返す |
| `executors` | 既定の動作実行器の連鎖 |
| `menuItems` | メニュー項目の定義 |

`menu.json` は UTF-8（BOM 可）で、`third_party/nlohmann/json` で解析します。項目の型違い、未知のゲート／実行器、未知の `actionType`、重複 id などの文書レベルの誤りは、組み込みメニューへ戻します。未知の連鎖名は黙って無視しません。

連鎖の上書き規則：項目ごとに明示した連鎖は、ルートの既定に**追加するのではなく置き換え**ます。ルートの既定は、連鎖が無いか空のときだけ効きます。

### 項目ごとの項目

| 項目 | 説明 |
|-------|-------------|
| `id` | 一意な識別子 |
| `label` | メニューの文字列。`&` はアクセスキー |
| `verb` | 文字列のコマンド名。InvokeCommand はこれか `canonicalName` で照合する |
| `helpText` | エクスプローラーのステータスバーに出す説明 |
| `canonicalName` | 正規のコマンド名。照合では `verb` と同義に扱う |
| `icon` | DLL のディレクトリからの相対パスで示す SVG/BMP |
| `targets` | `file`、`directory`、`directoryBackground`、`drive`、`fileSystemObject` |
| `separatorAfter` | 項目の後ろに区切り線を入れる |
| `extensions` | `[".cpp", ".h"]` のような許可拡張子 |
| `excludeExtensions` | 除外する拡張子 |
| `minSelection` / `maxSelection` | 選択数。`maxSelection: 0` は無制限 |
| `filesOnly` / `foldersOnly` | 選択された対象の種類を制限する |
| `actionType` | `messageBox` または `launch` |
| `actionTitle` / `actionTemplate` | メッセージボックスの題名と本文 |
| `actionCommand` / `actionShowWindow` | 起動コマンドとウィンドウの扱い |
| `extensionGates` / `itemGates` / `presentationGates` / `executors` | 項目ごとの連鎖の上書き。明示した連鎖はルートの既定を置き換える |

`targets` が空配列の場合は、従来の汎用の選択フィルタとの互換を保ちます。新しい項目では `targets` を明示してください。

### プレースホルダー

| トークン | 意味 |
|-------|---------|
| `%1` | 最初に選択した対象のフルパス。無ければ現在のディレクトリ |
| `%*` | 選択したすべてのパス（引用符で囲み、連結したもの） |
| `%D` | 最初に選択した対象の親ディレクトリ。無ければ現在のディレクトリ |
| `%N` | 最初に選択した対象（または現在のディレクトリ）の名前。親パスは含まない |

`menu.json` が無いか不正な場合、拡張は `include/shell_ext/common.h` の組み込み項目と既定のゲート連鎖へ戻ります。

プレースホルダーの展開は 1 回だけです。置換後の文字列を再走査しないため、`%1` や `%N` を含むファイル名を選んでも安全です。

### 起動コマンドの解決

`launch` の動作は、実行ファイルを `actionCommand` から明示的に解決してから起動します：

- 引用符で囲まれたパスはそのまま使う。
- 引用符が無く空白を含むパスは、CreateProcess の文書化された前方一致探索に従う。
- コマンド名だけの場合（`notepad.exe` など）は、システムのディレクトリと PATH の絶対パスからのみ解決する。**現在のディレクトリは決して探索しない**ので、同名の実行ファイルに乗っ取られない。
- 解決できないコマンドは起動せず、ログに残して拒否する。

### 長いパス

選択した対象のパスは `MAX_PATH` に縛られません。`%1`、`%*`、`%N`、`%D` はいずれも動的な `std::wstring` として扱います。ディレクトリの背景のパスはまず `SHGetPathFromIDListEx` を試し、だめなら `SHGetPathFromIDListW` に切り替えます。ファイル属性の取得は、通常のパスで失敗すると `\\?\` 付きの拡張パスで再試行します。

`MAX_PATH` を超える実パスには、システムの長いパスに関するポリシーと、長いパスを扱えるエクスプローラーが必要です。そうでない場合、Shell はそのようなパスを拡張に渡しません。

## プロジェクトの拡張

### メニュー項目を追加する

1. `config/menu.json` の `menuItems` にオブジェクトを足す。
2. 一意な `id`、`verb`、`targets`、フィルタ、動作の項目を設定する。
3. SVG/BMP を `config/icons/` に置き、相対パスで参照する。
4. `python tools/validate_menu_json.py` を実行する。

### ゲートを追加する

1. `src/gates/` に `IExtensionGate`、`IMenuItemGate`、`IMenuItemPresentationGate` の実装を足す。
2. `GateRegistry::RegisterBuiltInGates` に名前を登録する。
3. `menu.json` の適切な連鎖でその名前を使う。

Extension ゲートは拡張が関与するか、Item ゲートは候補に入るか、Presentation ゲートは `Hidden`、`Disabled`、`Enabled` のどれを返すかを決めます。

### 実行器を追加する

1. `src/actions/` に `IActionExecutor` の実装を足す。
2. `ExecutorRegistry::RegisterBuiltInExecutors` に登録する。
3. ルートか項目ごとの `executors` 配列に名前を足す。
4. 新しい設定項目が必要なら `MenuAction` と `MenuConfig` を拡張する。

### Shell ターゲットを追加する

1. `ShellTargetType` に列挙値を足す。
2. `ParseShellTargetType` と `GetShellTargetRegistryFileType` を更新する。
3. `ContextBuilder` に実行時の判定を足す。
4. ターゲットの照合と、登録／解除の既知リストを更新する。
5. `tools/validate_menu_json.py`、README、アーキテクチャ文書を更新する。

### COM の登録を変える

登録の入口は `src/extension/dllmain.cpp`、レジストリの補助は `src/registry/Reg.cpp` にあります。部分的な登録失敗時のロールバック、解除の冪等性、旧ターゲットの掃除を保ってください。削除してよいのは本プロジェクトの CLSID のサブキーだけで、`ContextMenuHandlers` の親キーごと消してはいけません。

## デバッグとトラブルシューティング

- DebugView で `[ShellExt]` のログを見る。
- DLL のビット数をエクスプローラーに合わせる。
- DLL や設定を変えたら再登録する。必要ならエクスプローラーを再起動する。
- メニューが出ないときは、登録ターゲット、項目の `targets`、`filesOnly`／`foldersOnly`、ゲートの連鎖をまとめて確認する。
- 登録に失敗するときは、昇格、`registration.json`、`regsvr32` のビット数を確認する。

## プロジェクト構成

```text
config/                 実行時のメニュー、登録ターゲット、アイコン
src/extension/          COM DLL の入口、ClassFactory、FileContextMenuExt、ComGuard
src/context/            選択の文脈と Shell ターゲットの判定
src/gates/              ゲートのインターフェース、レジストリ、組み込みゲート
src/actions/            実行器のインターフェース、レジストリ、動作
src/icons/              SVG/BMP アイコンのプロバイダ
src/menu/               設定、フィルタ、挿入、呼び出し
src/registry/           COM/Shell のレジストリ補助
tools/                  ビルド補助、登録スクリプト、検証
scripts/                リポジトリ規則のチェッカーとそのテスト
tests/                  CTest の単体テスト
third_party/            サードパーティ（nlohmann/json、NanoSVG）
docs/                   アーキテクチャと検証のドキュメント
```

## 注意

- 64 ビットのエクスプローラーには 64 ビットの DLL が必要です。32 ビットのエクスプローラーでは対応するビット数を使ってください。登録スクリプトは `System32`／`SysWOW64` の `regsvr32` を自動で選びます。
- COM の登録には管理者権限が必要です。
- Windows 7 への対応は設計上の目標です。リリース前に Win7 SP1 x64 での回帰を済ませてください。現在の VS 2026 ツールチェーンと静的 CRT は Win7 で別途検証が必要です。非対応なら、対応する古いツールチェーンで再ビルドしてください。
- DLL は MSVC の CRT を静的リンクします。新しい依存を足すときはランタイム要件を見直してください。
- `build/`、`build-*`、サードパーティのディレクトリにある生成物は編集しないでください。

## COM の同一性とライセンス

フォークを配布するときは、`src/extension/dllmain.cpp` の `CLSID_FileContextMenuExt` と、`include/shell_ext/common.h` の表示名を変えてください。

本プロジェクトは Microsoft のサンプルを出発点にしています。ソースファイルは元の Microsoft のヘッダーを保ち、[Ms-PL](https://www.microsoft.com/opensource/licenses.mspx#Ms-PL) に従います。ライセンス全文は `LICENSE` に、nlohmann/json と NanoSVG の告知は `THIRD_PARTY_NOTICES.md` にあります。
