# TODO — KBM per-keyboard remap

> 粒度: コンテキストリセット後でも再開できるように書く。完了は `[x]`。

## Phase 0: 準備（済/進行中）
- [x] upstream に類似 Issue/PR がないか調査（#12349 OPEN 本家 / #1460 CLOSED / #35632 dup、関連 PR なし）
- [x] 独立ブランチ作成 `feature/kbm-per-keyboard-remap`（origin/main 起点）
- [x] fork へ push（`origin/feature/kbm-per-keyboard-remap`）
- [x] 作業ドキュメント一式作成（PLAN/SPEC/TODO/KNOWLEDGE/README）
- [ ] ドキュメントをコミット & push

## Phase 1: 仕様確定（済 2026-07-10）
- [x] 確認事項 #1 種別定義: レイアウト＋物理個体（Mac/Windows）両方
- [x] 確認事項 #2 抑制: 不要（プロファイル切替方式）→ 方式 **D'** 採用
- [x] 確認事項 #3 切替対象: KBM のプロファイル（remap 一式）を切替
- [x] 確認事項 #4 UI: 手動切替(MVP) + 自動検出(拡張)。#35632 の A/B/all ドロップダウン踏襲
- [x] 方式確定: **D'（プロファイル切替、手動 MVP＋Raw Input 自動 拡張）**

## Phase 2: PoC（進行中）
- [x] 既存 KBM コード構造の把握 → activeConfiguration 機構＋ホットリロードが既存（POC-RESULTS.md §2）
- [x] Raw Input でキーボード列挙 PoC（`enum_keyboards.cpp` ビルド＆実行）
      → 実機で Apple(05AC)/Logitech(046D) を判別。USB/BT 両形式パース対応（POC-RESULTS.md §1）
- [x] デバイス安定識別子: **RIDI_DEVICENAME パスを主キーに確定**（VID/PID 無しデバイスも安定区別。表示用に VID/PID/製品名）
- [x] `WM_INPUT` でアクティブキーボード検出 PoC（`detect_active_keyboard.cpp`）→ 実機で Apple⇔2台目を判別（POC-RESULTS.md §4）
- [x] 2台同時接続の設計方針を確定（SPEC §7）: 手動MVP＋自動はヒステリシス、NULL注入無視、列挙＋実打鍵で学習 等
- [ ] 自動 vs 手動の**既定**をユーザーと確定（現状 手動MVP＋自動任意 を推奨）
- [ ] 仮想/合成キーボード除外・コレクション重複排除のロジック確認

## Phase 3: 実装（計画済 → [IMPL.md](IMPL.md)）
- [x] 切替機構をコードで検証: `activeConfiguration` 書換＋イベント signal でエンジンがライブ切替（エンジン改造不要）
- [x] dev 環境を worktree 分離（`C:/dev/PowerToys-perkbd`）／既定=手動 確定
- [x] Step A: 切替機構を実機で検証 ✅（test.json A→B で稼働中エンジンがライブ切替、即復元。IMPL.md 参照）
- [x] Step B: プロファイル管理モデル `ProfileManager.cs`（列挙/作成/削除/切替＋event signal）
- [x] Step B2: SettingsManager をプロファイル対応（キャッシュ `editorSettings.{profile}.json`＋`ReloadForActiveProfile`）
- [x] Step C: 切替アクション（C# → settings.json 書換＋event signal。ProfileManager 内）
- [x] Step D: Editor UI（プロファイル選択 ComboBox＋新規/削除ダイアログ＋配線＋resw 多言語）
- [x] **ビルド緑**（worktree、KeyboardManagerEditorUI.dll、警告なし）
- [ ] Step E: 実機テスト（エディタ起動→作成/切替→remap がプロファイル単位で効くか。安定版を一旦止めて dev で。要ユーザー）

## Phase 3.5: MVP 後の磨き込み
- [ ] 新規作成の名前バリデーションを UI にインライン表示（現状は失敗時 log のみ）
- [ ] プロファイル改名機能
- [ ] settings.json 書込競合（PowerToys Settings と共有）の検証
- [ ] editorSettings.json(default)→プロファイル cache の後方互換を実機確認
- [ ] 自動切替（Engine に Raw Input + デバイス→プロファイル対応表）＝別フェーズ
- [ ] 単体テスト / devdocs 追記

## Phase 4: コントリビュート（ブロッカー: CLA / 会社 IP 判断）
- [ ] #12349 に設計コメント / 実装意向を投稿
- [ ] upstream/main へリベース（現状 origin/main 起点で ~36 コミット遅れ）
- [ ] 作業ノート `kbm-per-keyboard-notes/` を PR から除外/整理
- [ ] PR 作成
- [ ] CLA 対応（会社 IP 判断が出てから／#49136 と共通ブロッカー）
