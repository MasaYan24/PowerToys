# IMPL — 手動切替 MVP 実装計画（2026-07-11）

対象 worktree: `C:/dev/PowerToys-perkbd`（branch `feature/kbm-per-keyboard-remap`）。

## 0. 検証済みの土台（実コード確認）

**プロファイル切替の仕組みはエンジンに既存。改造不要でライブ切替できる。**

- エンジンは `PowerToys_KeyboardManager_Event_Settings` を監視（`KeyboardManagerEngineLibrary/KeyboardManager.cpp:73`, コールバック `:38-70`）。
- signal されると `LoadSettings()`（`:49,76`）→ `state.LoadSettings()`（`common/MappingConfiguration.cpp:410`）。
  - `LoadSettings()` は毎回 **settings.json の `activeConfiguration` を読み**（`:416`）、`{その名前}.json` をロード（`:426`）。
  - remap 有無で LL フックを自動 start/stop（`KeyboardManager.cpp:62-69`）。

→ **切替 = (1) settings.json の `activeConfiguration` を別名に書換 → (2) 上記イベントを signal。** これだけでエンジンが別プロファイルを適用する。

## 1. MVP スコープ（手動のみ / 既定 = 手動）

- 複数プロファイル（= 複数の `{name}.json` 設定ファイル）を持てる。
- Editor でアクティブプロファイルを選択・作成・改名・削除できる。
- 選択で `activeConfiguration` を書換＋イベント signal → 即時反映。
- **自動切替（Raw Input）・デバイス対応表は次フェーズ**（このMVPには含めない）。

## 2. 実装ステップ（小さく縦切り）

### Step A: 切替機構の実地検証（コード前・手動）
- KBM フォルダに `default.json` と `mac.json`（別 remap）を用意。
- settings.json の `activeConfiguration` を `mac` に書換 → イベント signal（小ツール or 既存 Save 経路）。
- **エンジンが mac.json の remap に切り替わることを実機確認**（stable PT を止めて dev で）。
- → 土台前提の最終確証。ダメなら設計見直し。

### Step B: プロファイル管理モデル（Editor/C#）
- プロファイル一覧 = KBM フォルダ内の `*.json`（`editorSettings.json` は除外）。アクティブ = settings.json の `activeConfiguration`。
- 既存の未配線 `EditorSettings.ProfileDictionary`/`ActiveProfile`（`KeyboardManagerEditorUI/Settings/EditorSettings.cs:14/18`）をこの用途に配線するか、ファイル走査ベースにするか決める（要検討）。
- 注意: WinUI3 Editor は独自の `editorSettings.json` を持ち、ネイティブ設定と `KeyboardMappingService` 経由で同期する。**プロファイルごとに engine 側 `{name}.json` を分ける**設計との整合が最大の実装ポイント。

### Step C: 切替アクション（C# → settings.json + event）
- 選択プロファイル名を settings.json の `activeConfiguration` に書込む経路を実装（PowerToys 設定 API 経由）。
- `PowerToys_KeyboardManager_Event_Settings` を signal（`SaveSettingsToFile` が既に signal する経路を再利用できるか確認）。

### Step D: Editor UI
- `Pages/MainPage.xaml(.cs)` にプロファイル選択 ComboBox＋「新規/改名/削除」。
- 選択変更で Step C を呼ぶ。現在のマッピング表は選択プロファイルの内容を表示。

### Step E: テスト
- worktree でビルド（**初回は solution レベルの package restore が必要**）。
- stable PT を終了 → dev ビルド起動 → プロファイル作成・切替が効くか確認 → stable 復帰。
- KBM config は事前バックアップ。

## 3. 未解決の実装論点
- Editor の `editorSettings.json`（独自ストア）と engine の `{name}.json`（プロファイル実体）の対応をどう持つか。プロファイル切替時に Editor 側ストアもプロファイル単位で分ける必要があるか。
- settings.json の `activeConfiguration` を Editor プロセスから安全に書く方法（誰が所有権を持つか、書込競合）。
- 既存 UI（単一 default 前提）への影響を最小化する差し込み方。

## 4. 参考（file:line, 2026-07-11 worktree）
- 切替土台: `common/MappingConfiguration.cpp:410`(LoadSettings)/`:416`(activeConfiguration)/`:426`(config path)/`:450`(Save+signal)。
- エンジン監視: `KeyboardManagerEngineLibrary/KeyboardManager.cpp:73`/`:38-70`/`:76`。
- 既定名 "default": `common/KeyboardManagerConstants.h`。
- Editor 足場: `KeyboardManagerEditorUI/Settings/EditorSettings.cs:14/18`, `Settings/SettingsManager.cs`, `Interop/KeyboardMappingService.cs`, `Pages/MainPage.xaml(.cs)`。
