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
- [x] デバイス安定識別子: 第一候補 VID/PID に確定気味（同型2台はデバイスパス併用）
- [ ] `WM_INPUT` でアクティブキーボード検出の最小 PoC（自動切替トリガ検証。message-only window 新設が要る）
- [ ] 仮想/合成キーボード除外・コレクション重複排除のロジック確認

## Phase 3: 実装（未着手）
- [ ] **プロファイル概念の導入**（現状は単一 default.json）: プロファイル一覧＋アクティブプロファイル
- [ ] 設定スキーマ（プロファイル、デバイス→プロファイル対応表）設計
- [ ] Engine: アクティブプロファイル参照で remap 適用（既存フック経路に最小介入）
- [ ] Engine: Raw Input 登録＋`WM_INPUT` 受信でプロファイル自動切替（拡張）
- [ ] Editor(WinUI3): プロファイル選択ドロップダウン、検出キーボード一覧、手動切替
- [ ] 単体テスト
- [ ] devdocs 追記

## Phase 4: コントリビュート（ブロッカー: CLA / 会社 IP 判断）
- [ ] #12349 に設計コメント / 実装意向を投稿
- [ ] upstream/main へリベース（現状 origin/main 起点で ~36 コミット遅れ）
- [ ] 作業ノート `kbm-per-keyboard-notes/` を PR から除外/整理
- [ ] PR 作成
- [ ] CLA 対応（会社 IP 判断が出てから／#49136 と共通ブロッカー）
