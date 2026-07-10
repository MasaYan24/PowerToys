# PoC 結果 — Raw Input キーボード列挙 & KBM 構造調査（2026-07-10）

## 1. 列挙 PoC（`enum_keyboards.cpp`）— 実機結果

ビルド: `cl /utf-8 /EHsc /std:c++17 enum_keyboards.cpp user32.lib hid.lib`（ウィンドウ不要、`GetRawInputDeviceList` 使用）。

この開発機で検出された「キーボード」4件:

| # | 種別 | VID/PID | Product | Vendor | 備考 |
|---|---|---|---|---|---|
| 0 | 物理 | **05AC** / 0239 | Apple Wireless Keyboard | Apple Inc. | Bluetooth。Apple VID=05AC |
| 1 | 物理 | **046D** / C232 | (none) | (none) | USB。046D=Logitech、C232=Unifying受信機。HID文字列は非公開 |
| 2 | 仮想 | (n/a) | HID VHF Driver | Microsoft | Virtual HID Framework（BTスタック等） |
| 3 | 仮想 | (n/a) | ConvertedDevice | (none) | 合成デバイス（8キー、consumer control等） |

### 実証できたこと
- **物理キーボードごとに区別できる識別子が取れる**。ユーザーの Mac(Apple 05AC) と Windows(Logitech 046D) は **VID で確実に判別可能**。
- 識別子ソースは2系統:
  - **HID Product/Vendor 文字列**（`HidD_GetProductString`/`ManufacturerString`）: 人間可読。Apple では取得成功、Logitech受信機では空。
  - **VID/PID（デバイスパスから抽出）**: USB は `VID_046D&PID_C232`、**Bluetooth は `..._VID&000205ac_PID&0239`**（`0002`はソース、後半4桁`05ac`が実VID）。両形式のパースを実装済み。
- ウィンドウ無しで列挙可能（設定 UI で「検出キーボード一覧」を出すのはこれで足りる）。

### 実装時に効いてくる注意点（PoC で判明）
1. **Bluetooth の VID/PID 形式が USB と違う** → パーサは両対応必須（実装済み）。
2. **仮想/合成キーボードが混じる**（VHF, ConvertedDevice）→ プロファイル対応 UI では除外 or 明示が要る。VID/PID 無しを弾く簡易フィルタが有効。
3. **1つの物理デバイスが複数コレクション（`&Col01` 等）で列挙されうる** → デバイスパスのインスタンス部での重複排除が要る（今回の実機では実害なし）。
4. Logitech 受信機のように **HID 文字列が空**のケースがある → 表示名は「Vendor名(VID)+PID」へフォールバックする。
5. **安定識別子の推奨**: 第一候補 = VID/PID（モデル単位）。同型2台を区別する必要が出たら、デバイスパスのインスタンス部やシリアルを併用（BT はシリアル欠落あり）。

## 2. KBM 既存コード構造（実装の土台。file:line は 2026-07-10 時点）

### 決定的発見: 「named 設定」の仕組みが既に存在する
- 設定ファイルは固定の `default.json` ではなく **`{activeConfiguration}.json`**。アクティブ名は module の `settings.json` の `"activeConfiguration"` キーから読む。
  - 読込パス生成: `common/MappingConfiguration.cpp:426`、保存パス: `:642`、既定名 `"default"`: `common/KeyboardManagerConstants.h:80`。
- ロード/セーブ: `MappingConfiguration::LoadSettings()` (`MappingConfiguration.cpp:410`) / `SaveSettingsToFile()` (`:450`)。
- **保存時に `PowerToys_KeyboardManager_Event_Settings` を signal → エンジンがホットリロード**（`MappingConfiguration.cpp:652`、エンジン側コンストラクタ登録 `KeyboardManager.cpp:73`、再ロード `LoadSettings()` `:76`）。
- → **「プロファイル切替 = activeConfiguration を書き換えて reload イベントを叩く」だけで、切替の土台は既にある。** 新規に config 永続化機構を作る必要はない。

### エンジンのフックと入口
- LL フック設置: `KeyboardManager::StartLowlevelKeyboardHook()` → `SetWindowsHookEx(WH_KEYBOARD_LL,...)` (`KeyboardManager.cpp:140`)。判定: `HandleKeyboardHookEvent` (`:187`)。
- 実際の remap 適用: `KeyboardEventHandlers.cpp`（`HandleSingleKeyRemapEvent:92`、`HandleOSLevelShortcutRemapEvent:1759` 等）。
- ランタイムのマップは `State`（`State : public MappingConfiguration`, `State.h:5`）が保持。
- **エンジンにウィンドウは無い**。入口 `KeyboardManagerEngine/main.cpp:17`、ループは `run_message_loop`（`common/utils/window.h:13`）でスレッドメッセージのみ（`hwnd=nullptr`）。
  → **`WM_INPUT` はウィンドウ WndProc にしか来ない**ので、自動検出には **message-only window の新設 + `RegisterRawInputDevices`** が必要。
- 参考実装: FancyZones が Raw Input 使用 — `fancyzones/FancyZonesLib/KeyboardInput.cpp:11`（`RIDEV_INPUTSINK`、`RegisterRawInputDevices:17`、`OnKeyboardInput:26`、`GetRawInputData:30`）。

### Editor(WinUI3/C#) 側の足場
- 既に **nascent なプロファイル概念**あり（未配線）: `EditorSettings.cs:14 ProfileDictionary`, `:18 ActiveProfile`（別ファイル `editorSettings.json`）。
- 追加候補: プロファイル選択 ComboBox / 検出キーボード一覧 → `Pages/MainPage.xaml(.cs)`。ネイティブ橋渡し `Interop/KeyboardMappingService.cs`。永続化 `Settings/SettingsManager.cs`。

## 3. これを踏まえた実装方針（改訂）

- **MVP（手動切替）は既存機構の再利用で軽い**:
  1. プロファイル = 既存の `{name}.json`（activeConfiguration）をそのまま流用。
  2. Editor に「プロファイル選択＋作成」UI を追加し、選択で `activeConfiguration` を書き換え → reload イベント。
  3. デバイス→プロファイル対応表（VID/PID キー）を editorSettings 側に持つ。
- **拡張（自動切替）**: エンジンに message-only window を新設 → `RegisterRawInputDevices(RIDEV_INPUTSINK)` → `WM_INPUT` で打鍵デバイスの VID/PID を取得 → 対応表を引いて activeConfiguration を切替 → reload。FancyZones を雛形にする。
  - 既知の制約は SPEC.md §0 の通り（切替直後の最初の1打鍵レース）。
- ドライバは不要（抑制は従来 LL フックのまま。per-keyboard はアクティブプロファイルの切替で表現）。

## 4. 生成物
- `poc/enum_keyboards.cpp` — 列挙 PoC ソース（両 VID/PID 形式対応）。
- `poc/enum_keyboards.exe` — ビルド済み（実行して上表を取得）。※バイナリは Git 追跡外推奨。
