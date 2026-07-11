# KNOWLEDGE — KBM per-keyboard remap

ハマりポイント・気づきを即記録。次回同じミスをしない。

## 技術
- **hook vs HID の壁**: `WH_KEYBOARD_LL` は抑制○/デバイス識別×、Raw Input は抑制×/デバイス識別○。
  「デバイス別に抑制付き remap」は標準 API では不可能で、フィルタドライバ（Interception 等）が必要。
  → これが upstream #1460 が実質保留になった根本理由。設計はここから逃げられない。SPEC.md 参照。
- 抑制が不要な「レイアウト/プロファイル自動切替」なら Raw Input のみで現実的（#12349 の方向）。
- デバイス識別子: VID/PID は“モデル”単位。同型2台の区別にはシリアル or デバイスパスが要る。BT はシリアル欠落することあり。

## 深掘り最終結論（2026-07-12）— 自動切替の全謎が解けた

- **最重要: LL フックが抑制（suppress）したキーは Raw Input に届かない。**
  決定的実験: Apple 8×5 + TC 8×5 + Enter の11打鍵中、エンジンに届いたのは 3打鍵のみ
  （default で押した Apple の 8×2 と、remap されていない Enter）。mac(8→9) が有効になった後の
  `8` は全て KBM 自身のフックが抑制 → Raw Input に不可視 → 自動切替の判定材料にならない。
  - 帰結: **「アクティブプロファイルで remap されたキーだけを叩く」限り切替は原理的に発火しない**。
    通常のタイピング（大半のキーは remap されていない）では2打で確実に発火（`qwe8` テストで実証）。
  - 過去の「戻らない」「たまに戻る」は全てこれで説明がつく（a→b テストで 'a' だけ叩いていた）。
- **修飾キー（Shift/Ctrl/Alt/Win）はヒステリシスから除外する**（実装済み）。
  ホットキーチョード自身の Shift↓Alt↓ が自動切替のカウントに入り、巡回切替と喧嘩して
  「余分な切替」が起きた（9898 のはずが 99899899）。修飾キー除外で完全解消（98989898 を実証）。
- **ホットキー巡回切替 実装・実証済み**: `deviceProfiles.json` の `cycleHotkey`（{win,ctrl,alt,shift,code}）。
  `ProfileCycleHotkey`（専用スレッド+隠しウィンドウ+RegisterHotKey+MOD_NOREPEAT）→ `CycleActiveProfile()` が
  keyboardConfigurations を巡回 → 既存 SwitchActiveProfile 経路＋MessageBeep。未定義なら無効（安全）。
  エディタの DeviceProfileManager は cycleHotkey を JsonNode でラウンドトリップ保持（Save で消さない）。
- **⚠️ dev エンジン（alone 未対応）×日常 default.json の危険**: `162→26 (condition:alone)` を
  「無条件 remap」と誤解釈 → Ctrl(=CapsLock も) を保持すると up イベント不整合で **OS の Ctrl がスタック**
  （要再起動）。dev エンジンでのテスト中は Ctrl/CapsLock/Win 刻印キーの保持を避けること。
  alone ブランチと統合すれば解消。
- **この PC は registry Scancode Map で modifier を入替済み**（全キーボード共通・ドライバレベル）:
  CapsLock→LCtrl / Ctrl(L)→LWin / Win(L)→LCtrl / Ctrl(R)→RWin / Win(R)→RCtrl / Menu→RCtrl。Alt は無変更。
  Raw Input は Scancode Map 適用後の VK を見るので、検出・ホットキーとも整合する。
- 長押し（オートリピート）は Raw Input には1打鍵（HID レポートは press 1回のみ）。連打テストは「トントン」で。

## 自動切替（エンジン Raw Input）実機テストで判明したこと（2026-07-11）

- **デバイスパスは必ずしも安定ではない（重要）**: 仮想プロバイダ系キーボード（実機の2台目 = `Target_KIP&Category_HID`）は
  RIDI_DEVICENAME の **インスタンスID が時々変わる**（`...Col01#4&1d10d7d2&0&0000#...` ↔ `...#4&31ee05e0&0&0000#...`）。
  フルパス完全一致だと、変わった瞬間に未登録扱い→切替されず**プロファイルが固まる**（「たまに戻る/ほぼ戻らない」の正体）。
  → **対策: マッチングを安定プレフィックス（2番目の `#` まで＝インスタンスID を除く）に正規化**（`NormalizeDevicePath`）。
    `\\?\HID#Target_KIP&Category_HID&Col01` や `\\?\HID#{container}_VID&..._PID&...&Col01` で一致させる。
  → **副作用（既知の制約）**: 同一モデル2台はインスタンスID でしか区別できないため正規化で同一視される。MVP は許容、SPEC 制約に追記。
- **「キー保持中は切替保留」の集合追跡は仮想デバイスで破綻**: keyup が取りこぼされ pressedKeys が枯れず、`held>1` ガードが
  ヒステリシス加算より前にあったため **pending が進まず永久ブロック**。→ **defer-while-held を撤去し、ヒステリシス（連続N打鍵）のみ**に。
  defer は将来 `GetAsyncKeyState`（モディファイア実状態）で堅牢に再実装（Phase 3.5）。
- **診断手法**: `[autosw] target=.. current=.. pending=..xN requested=..` を毎打鍵 trace で出すと切替判定の内部状態が丸見え。
  原因特定に極めて有効だった（PR 前に削除予定）。`Detected keyboard: <path>` は device path 収集に有用。
- **エンジンの dev テスト手順**: 安定版 PowerToys（runner+engine）を止める→dev エンジン `x64\Debug\KeyboardManagerEngine\...exe` を
  `$env:PATH="...\x64\Debug;$env:PATH"` ＋ WorkingDirectory=x64\Debug で起動（依存 DLL のため）。単一インスタンス mutex があるので
  安定版エンジンとは同時起動不可。engine ログ: `%LOCALAPPDATA%\...\Keyboard Manager\Engine\Logs\vX\log_*.log`。
  復元: config を backup から戻す＋`Stop-Process -Name PowerToys*`＋安定版 `C:\dev\PowerToys\x64\Release\PowerToys.exe` を再起動。
- 自動切替 = 「settings.json の activeConfiguration 書換＋`PowerToys_KeyboardManager_Event_Settings` signal」で既存リロード経路を再利用（state を別スレッドで触らない）。実装で機能した。
- deviceProfiles.json 形式: `{"autoSwitchEnabled":bool,"map":[{"device":"<RIDI path>","profile":"<name>"}]}`。engine が LoadSettings のたびに再読込。

## エディタ実機テスト（Step E）で判明したこと
- **dev ビルドのエディタ起動には DLL パス対応が必須**。exe は `x64\Debug\WinUI3Apps\` にあるが、
  ネイティブラッパー `PowerToys.KeyboardManagerEditorLibraryWrapper.dll` とその依存(`PowerToys.Interop.dll`)は
  親の `x64\Debug\` にある → そのまま起動すると `0x8007007E`（module not found）で `_mappingService` が null 化。
  症状: マッピングが表示されない（「Nothing mapped yet」）＋切替ハンドラが `_mappingService==null` で早期 return（切替が効かない）。
  → **回避**: 起動前に `$env:PATH = "C:\dev\PowerToys-perkbd\x64\Debug;$env:PATH"` してから exe を起動（WorkingDirectory も x64\Debug に）。
  （製品版ではランナーがパスを解決するので問題にならない。dev 実行時のみ。）
- **切替時の表示クリア忘れバグ（修正済 634bedc3d）**: `LoadRemappings/LoadTextMappings/LoadProgramShortcuts/LoadUrlShortcuts` は
  そのタイプが空だと `.Clear()` の前に early return していた → 空プロファイルに切り替えると旧一覧が残る。
  → **null チェックの前に必ずリストを Clear** する。
- エディタは引数不要で単体起動可（`App.xaml.cs` OnLaunched が MainWindow を作るだけ）。ランナー/安定版エンジンを止める必要なし
  （稼働中エンジンがそのまま reload イベントに反応）。安定版の「設定エディタ」だけは同時に開かない。
- テスト時は毎回 config をバックアップ（`_stepE-backup-*`）。復元は settings.json を戻す＋reload イベント signal。

## プロファイル切替の実装知識（Step A で実証）
- **切替 = settings.json の `activeConfiguration.value` を別名に書換 → イベント signal**。エンジンは即ライブ切替（改造不要）。
- イベント名: **`PowerToys_KeyboardManager_Event_Settings`**（`common/KeyboardManagerConstants.h` の `SettingsEventName`）。
  PowerShell から: `[System.Threading.EventWaitHandle]::OpenExisting("PowerToys_KeyboardManager_Event_Settings").Set()`（同一ユーザーセッションなら prefix 不要）。
- settings.json 位置: `%LOCALAPPDATA%\Microsoft\PowerToys\Keyboard Manager\settings.json`。
  既に `keyboardConfigurations.value`（プロファイル名の配列）と `activeConfiguration.value` を持つ。プロファイルの器は既存。
- 各プロファイルの実体 = 同フォルダの `{name}.json`（remap 一式）。`default.json` が既定。
- エンジンはファイル監視ではなく**イベント監視**（`KeyboardManager.cpp` の `settingsEventWaiter`）。書換だけでは反映されず、signal 必須。
- Step A テストは**稼働中の安定版エンジンで実施可能**（この機構は全ビルド共通）。config は必ず事前バックアップ＆即復元。

## リポジトリ運用
- **別機能は「別レポジトリ」ではなく「同 fork の別ブランチ」**。PowerToys はモノレポなので機能単位で切り出せない。
- 本ブランチは `origin/main`(3d3cef73d) 起点。**upstream/main 起点にできなかった理由 ↓**。
- **push が workflow スコープで拒否される**:
  `refusing to allow an OAuth App to ... workflow .github/workflows/... without workflow scope`
  → `gh auth token` の OAuth トークンに `workflow` スコープが無い。upstream/main 最新にはワークフローファイル差分が含まれ、それを含むブランチを fork へ push できない。
  → **回避策**: 起点を `origin/main`（fork の main、= リモートに既存のコミット）にすると新規オブジェクトにワークフロー差分が入らず push 可能。
  → PR 確定前に `upstream/main` へリベースする際は、workflow スコープ付き認証（ユーザー自身の git / SSH / `gh auth refresh -s workflow`）で push すること。

## サンドボックス環境
- **Bash ツールは壊れている**（fork `0xC0000142`）。必ず PowerShell(pwsh7) を使う。
- **SSH push 不可**（`git@github.com: Permission denied (publickey)`、鍵なし）。
  → push は `gh auth token` で HTTPS basic 認証:
  `git -c credential.helper= push "https://x-access-token:$t@github.com/MasaYan24/PowerToys.git" <branch>:<branch>`
  出力からトークンをスクラブする（`-replace`）。
- fetch も SSH 経由なので、push 後にローカル remote-tracking を手動更新:
  `git update-ref refs/remotes/origin/<branch> <sha>` → その後 `--set-upstream-to` 可能。
- `git reset --hard` はオートモードのクラシファイアに拒否されることがある。
  作業ツリーがクリーンならブランチ移動は `git switch -C <branch> <base>` で代替可能。

## コントリビュート
- CLA 未対応。会社(DENSO)の IP 判断待ち。個人 or 会社が確定するまで CLA 同意コメントは投稿しない（#49136 と共通）。
- コミット著者は `MasaYan24 <7567050+MasaYan24@users.noreply.github.com>` 固定。会社メール混入厳禁。

## 作業ノートの扱い
- この `kbm-per-keyboard-notes/` は個人作業用。upstream PR には含めない（PR 化前に除外/整理）。
