# KBM per-keyboard remap — 作業ノート

PowerToys Keyboard Manager に「**キーボードの種類に応じて割当を変える**」機能を追加するための、
個人作業用ドキュメント一式（`feature/kbm-per-keyboard-remap` ブランチ）。

## 概要（何をするものか）
複数の物理キーボードを使い分ける環境で、KBM の remap を固定ではなく
「どのキーボードで打鍵しているか」に応じて切り替えられるようにする。

- 例: 内蔵キーボード＝プロファイルA、外付け US 配列＝プロファイルB を自動切替。
- upstream の要望 **#12349（OPEN, 本家）/ #1460 / #35632** に対応する機能。

## 現状
- **仕様確定前（実装未着手）**。方式は SPEC.md の A〜D で未決。
- 最大の論点は「hook は抑制できるがデバイス識別不可 / HID は識別できるが抑制不可」という技術的壁。
  抑制不要なプロファイル自動切替（方式 D, #12349 相当）が現実的な第一歩。

## ドキュメント構成
| ファイル | 役割 |
|---|---|
| [PLAN.md](PLAN.md) | 目的・背景・制約・ユーザー確認事項・既存 Issue 調査結果 |
| [SPEC.md](SPEC.md) | 技術アプローチ A〜D の壁打ち、推奨スコープ |
| [TODO.md](TODO.md) | フェーズ別タスク（Phase 0 準備〜Phase 4 コントリビュート） |
| [KNOWLEDGE.md](KNOWLEDGE.md) | ハマりポイント（hook/HID の壁、push の workflow スコープ制約、環境） |

## ブランチ / リポジトリ
- ブランチ: `feature/kbm-per-keyboard-remap`（`origin/main` 3d3cef73d 起点、独立）
- fork: `MasaYan24/PowerToys`、upstream: `microsoft/PowerToys`
- 現行の alone/tap PR #49136（`feature/kbm-dual-key-alone`）とは**別機能・依存なし**。

## 次のアクション
PLAN.md の「ユーザー確認事項」4点を確定 → SPEC.md の方式を1つに決める → PoC。
詳細は TODO.md 参照。

> このフォルダは作業用メモであり、将来の upstream PR には含めない想定。
