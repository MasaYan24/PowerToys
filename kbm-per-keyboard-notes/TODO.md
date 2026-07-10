# TODO — KBM per-keyboard remap

> 粒度: コンテキストリセット後でも再開できるように書く。完了は `[x]`。

## Phase 0: 準備（済/進行中）
- [x] upstream に類似 Issue/PR がないか調査（#12349 OPEN 本家 / #1460 CLOSED / #35632 dup、関連 PR なし）
- [x] 独立ブランチ作成 `feature/kbm-per-keyboard-remap`（origin/main 起点）
- [x] fork へ push（`origin/feature/kbm-per-keyboard-remap`）
- [x] 作業ドキュメント一式作成（PLAN/SPEC/TODO/KNOWLEDGE/README）
- [ ] ドキュメントをコミット & push

## Phase 1: 仕様確定（ブロッカー: ユーザー回答待ち）
- [ ] PLAN.md 確認事項 #1「キーボード種別の定義」を確定（レイアウト / 物理個体 / 内蔵外付け）
- [ ] 確認事項 #2「抑制の要否」を確定 → 方式 B/C/D の分岐
- [ ] 確認事項 #3「切替対象（KBM remap / OS レイアウト / IME）」を確定
- [ ] 確認事項 #4「UI 期待値（A/B/all ドロップダウン）」を確定
- [ ] 方式を SPEC.md の A〜D から1つに確定

## Phase 2: PoC（方式確定後）
- [ ] （方式 D の場合）`WM_INPUT` でアクティブキーボード検出の最小 PoC
- [ ] デバイス安定識別子の実験（VID/PID/シリアル/デバイスパスの取得可否）
- [ ] （方式 B を検討する場合）“直近デバイス”ヒューリスティックのレース検証

## Phase 3: 実装（未着手）
- [ ] 設定スキーマ（プロファイル ↔ デバイス対応表）設計
- [ ] Engine 実装
- [ ] Editor(WinUI3) UI 実装
- [ ] 単体テスト
- [ ] devdocs 追記

## Phase 4: コントリビュート（ブロッカー: CLA / 会社 IP 判断）
- [ ] #12349 に設計コメント / 実装意向を投稿
- [ ] upstream/main へリベース（現状 origin/main 起点で ~36 コミット遅れ）
- [ ] 作業ノート `kbm-per-keyboard-notes/` を PR から除外/整理
- [ ] PR 作成
- [ ] CLA 対応（会社 IP 判断が出てから／#49136 と共通ブロッカー）
