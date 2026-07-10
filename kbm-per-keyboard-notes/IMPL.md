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

### Step A: 切替機構の実地検証（✅ 完了 2026-07-11）
- **稼働中の安定版エンジン（ビルド不要）で実証成功。**
- 手順（実施済み）:
  1. KBM フォルダを丸ごとバックアップ。
  2. `test.json` を作成（`{"remapKeys":{"inProcess":[{"originalKeys":"65","newRemapKeys":"66"}]},...}` = A→B のみ）。
  3. `settings.json` の `activeConfiguration.value` を `"test"` に書換、`keyboardConfigurations.value` に `"test"` 追加。
  4. 名前付きイベント **`PowerToys_KeyboardManager_Event_Settings`** を signal（.NET `EventWaitHandle.OpenExisting(name).Set()`）。
  5. → **`a` を打つと `b` が出力／日常 remap は消滅**を確認 = プロファイルが丸ごとライブ切替した。
  6. バックアップから settings.json 復元＋test.json 削除＋再 signal で**完全復帰**。
- **結論: エンジン改造ゼロでライブ切替できる。** MVP はこの経路（activeConfiguration 書換＋event signal）を Editor から呼ぶだけ。
- 既存の `keyboardConfigurations`（settings.json 内のプロファイル名一覧）が**プロファイルリストの器として既存**なのも確認。

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

## 2.5 設計確定: 手動切替 MVP は「C# エディタ側だけ」で完結（コード確認済み）

エディタの読み書き経路を確認（`KeyboardManagerEditorUI/Interop/KeyboardMappingService.cs`）:
- `new KeyboardMappingService()` → `KeyboardManagerInterop.CreateMappingConfiguration()` + `LoadMappingSettings()`
  → native `MappingConfiguration::LoadSettings()` = **activeConfiguration に従い `{name}.json` を読む**。
- `SaveSettings()` → native `SaveSettingsToFile()` = `{name}.json` 保存＋リロードイベント signal。
- エディタ自身の `editorSettings.json`（`SettingsManager.cs`）は**表示キャッシュ**で、native service から再構築できる
  （`CreateSettingsFromKeyboardManagerService()`）。

→ **エンジンもネイティブラッパーも改造不要。** プロファイル切替は C# 側で:
1. `SettingsManager._settingsDirectory\settings.json` の `activeConfiguration.value` を書換＋`keyboardConfigurations.value` に名前追加。
2. `EventWaitHandle.OpenExisting("PowerToys_KeyboardManager_Event_Settings").Set()` で signal（Step A と同手法）。
3. `KeyboardMappingService` を作り直し（新アクティブ設定を読む）→ `editorSettings.json` キャッシュを再構築 → UI 更新。
- 新規プロファイル作成 = `{name}.json`（空 or 複製）を作り、`keyboardConfigurations` に追加。

## 2.6 設計確定: プロファイル単位のエディタキャッシュ（その場しのぎ回避）

`MainPage.xaml.cs` / `SettingsManager.cs` 確認の結果:
- エディタの表示は `SettingsManager.EditorSettings`（= `editorSettings.json`）から構築される（`LoadAllMappings`）。
  このキャッシュは **単一共有ファイル**で、プロファイルの概念が無い。
- native `KeyboardMappingService` は**アクティブな `{name}.json`** を読み書きする（source of truth）。

**課題**: プロファイルを切り替えても `editorSettings.json` が旧プロファイルのまま → 表示不整合。

**確定方針（堅実解・その場しのぎ回避）**:
- **エディタキャッシュもプロファイル単位にする**: `editorSettings.json` → **`editorSettings.{profile}.json`**。
  engine の `{name}.json` と1対1対応させ、プロファイル間の混線を構造的に防ぐ。
- プロファイル切替の完全フロー（editor 内）:
  1. `ProfileManager.SetActiveProfile(name)`: settings.json の `activeConfiguration` 書換＋`keyboardConfigurations` 追加＋保存＋reload イベント signal。
  2. `_mappingService` を作り直す（新アクティブ `{name}.json` を読む）。
  3. `SettingsManager` のアクティブプロファイルを切替 → `editorSettings.{name}.json` を読む（無ければ native service から再構築して保存）。
  4. `LoadAllMappings()` で UI 更新。
- 新規プロファイル = 空の有効 config `{name}.json`（`{"remapKeys":{"inProcess":[]},"remapKeysToText":{"inProcess":[]},"remapShortcuts":{"global":[],"appSpecific":[]},"remapShortcutsToText":{"global":[],"appSpecific":[]}}`）を作成＋`keyboardConfigurations` 追加。

## 2.7 ProfileManager API（新規・UI 非依存の核）

`KeyboardManagerEditorUI/Settings/ProfileManager.cs`（新規, static）:
- `IReadOnlyList<string> GetProfiles()` — settings.json の `keyboardConfigurations` を読む（無ければ `{*}.json` 走査、常に `default` を含む）。
- `string GetActiveProfile()` — settings.json の `activeConfiguration`（既定 `default`）。
- `bool SetActiveProfile(string name)` — activeConfiguration 書換＋list 追加＋保存＋`SignalEngineReload()`。
- `bool CreateProfile(string name, bool copyFromActive)` — `{name}.json` 作成（空 or 複製）＋list 追加＋保存。
- `bool DeleteProfile(string name)` — `{name}.json` と `editorSettings.{name}.json` 削除＋list 除去。アクティブだったら `default` へ切替。
- `void SignalEngineReload()` — `EventWaitHandle.OpenExisting("PowerToys_KeyboardManager_Event_Settings").Set()`（Step A と同手法）。
- settings.json はスキーマ保全のため **JsonNode で外科的に**編集（未知プロパティを壊さない）。
- 既知の注意: settings.json は PowerToys Settings も所有。編集競合の可能性は今後の論点（§3）。

## 3. 未解決の実装論点
- Editor の `editorSettings.json`（独自ストア）と engine の `{name}.json`（プロファイル実体）の対応をどう持つか。プロファイル切替時に Editor 側ストアもプロファイル単位で分ける必要があるか。
- settings.json の `activeConfiguration` を Editor プロセスから安全に書く方法（誰が所有権を持つか、書込競合）。
- 既存 UI（単一 default 前提）への影響を最小化する差し込み方。

## 4. 参考（file:line, 2026-07-11 worktree）
- 切替土台: `common/MappingConfiguration.cpp:410`(LoadSettings)/`:416`(activeConfiguration)/`:426`(config path)/`:450`(Save+signal)。
- エンジン監視: `KeyboardManagerEngineLibrary/KeyboardManager.cpp:73`/`:38-70`/`:76`。
- 既定名 "default": `common/KeyboardManagerConstants.h`。
- Editor 足場: `KeyboardManagerEditorUI/Settings/EditorSettings.cs:14/18`, `Settings/SettingsManager.cs`, `Interop/KeyboardMappingService.cs`, `Pages/MainPage.xaml(.cs)`。

## 5. 自動切替（Engine Raw Input）— 実装リファレンス（2026-07-11）

新規/変更（`KeyboardManagerEngineLibrary`, tip d30f25ea6）:
- `RawInputKeyboardTracker.{h,cpp}`（新規）: 専用スレッドで隠しウィンドウ＋`RegisterRawInputDevices(RIDEV_INPUTSINK)` →
  `WM_INPUT` を受信し、`{devicePath, vkey, keyDown, injected}` をコールバック。抑制はしない。
- `KeyboardManager`:
  - `rawInputTracker` を ctor で生成・Start、dtor で Stop（コールバックは tracker スレッド）。
  - `LoadDeviceProfiles()`: `deviceProfiles.json`（`{autoSwitchEnabled, map:[{device,profile}]}`）を読み、
    `map[NormalizeDevicePath(device)] = profile` を保持。`LoadSettings()` のたびに再読込。
  - `OnRawKeyEvent()`: injected/keyup/パス無しは無視 → `NormalizeDevicePath` で map 照合 →
    `activeProfileName`(= `state.currentConfig`, LoadSettings で更新) と比較 → ヒステリシス（`AutoSwitchThreshold=2`）→
    `SwitchActiveProfile(target)`。
  - `SwitchActiveProfile()`: settings.json の activeConfiguration を JsonNode 相当（`json::JsonObject`）で書換 →
    `PowerToys_KeyboardManager_Event_Settings` を signal → **既存リロード経路**が新プロファイルを適用（state を別スレッドで触らない）。
  - `NormalizeDevicePath()`: RIDI_DEVICENAME を 2番目の `#` まで（インスタンスID を除く）に正規化。仮想デバイスの ID 揺れ対策。
- **状態**: 切替は実機で動作確認済み。正規化修正は実装済みだが**最終再検証は中断で保留**（TODO Phase 4a の★再開ポイント）。
- **未実装**: エディタ側の割当 UI（Increment 3）。今は `deviceProfiles.json` を手書き。
- **要削除（PR前）**: `[autosw]` 毎打鍵 trace（診断用）。
