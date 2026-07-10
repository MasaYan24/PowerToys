# PLAN — KBM per-keyboard remap（キーボード種別で割当を変える）

## 目的
Keyboard Manager の remap を「どの物理キーボードで打鍵したか」に応じて切り替えられるようにする。
例: 内蔵キーボードでは remap A、外付け US 配列キーボードでは remap B、など。

## 背景
- 現行 PR #49136（alone/tap 機能, branch `feature/kbm-dual-key-alone`）とは**別機能**。依存なし。
- 本ブランチ `feature/kbm-per-keyboard-remap` は `origin/main`(3d3cef73d) 起点で独立作成済み。
- ユーザーの動機: 複数キーボードを場面で使い分ける（JIS/US、内蔵/外付けドック等）ため、割当を固定ではなくキーボード単位にしたい。

## 既存の議論（upstream 調査済み・2026-07-10）
| # | タイトル | 状態 | 反応 | 位置づけ |
|---|---|---|---|---|
| **#12349** | [KBM] Auto-select keymap according to keyboard being typed on | **OPEN** (Idea-Enhancement, Product-KBM) | 👍8 | **本家トラッキング Issue**。ここに設計/PR を紐づける |
| **#1460** | [KBM] Multi Keyboard Support | CLOSED | 👍14 / 27コメント | 技術的壁を明言した最重要スレッド |
| #35632 | Save key remaps based on HID device (per-keyboard remap) | CLOSED (#12349 の重複) | — | UI 案（A/B/all ドロップダウン）あり |

- **関連する PR は現時点でゼロ** → 実装する価値あり／先行実装との衝突なし。

## 制約
- **技術的壁（最重要, #1460 より）**:
  > HID can differentiate devices but cannot suppress input. Hooks can suppress input but cannot differentiate devices.

  KBM が使う `WH_KEYBOARD_LL` は入力を**抑制できるがデバイス識別不可**。Raw Input/HID は**デバイス識別できるが抑制不可**。両立には Interception 等のフィルタドライバが必要。→ SPEC.md で詳細検討。
- Microsoft がドライバ同梱を受け入れる見込みは低い（配布・署名・セキュリティ面）。
- 貢献フロー: CLA 未対応（会社 IP 判断待ち。#49136 と同じブロッカー）。upstream PR 化はその後。
- コミット著者は `MasaYan24 <7567050+MasaYan24@users.noreply.github.com>` 固定。会社メールは混入させない。

## ユーザー確認事項（実装前に必ず詰める）
1. **「キーボード種別」の定義は？**
   - (a) キーレイアウト（JIS vs US など）で切替たい
   - (b) 物理デバイス個体（USB/BT の HID）で切替たい
   - (c) 内蔵 vs 外付け、の二分で十分
2. **抑制（元キーの無効化）が必要か？**
   - 必要 → ドライバ必須級の難題（#1460）
   - 不要（レイアウト/IME 自動切替だけ）→ Raw Input のみで現実的に可能（#12349 相当）
3. **切替対象は？** KBM の remap 定義そのものか、Windows 入力レイアウト/IME か。
4. UI 期待値: #35632 案（キーボード A/B/all のドロップダウンで編集対象を選ぶ最小 UI）で良いか。

→ 回答が出るまで実装は着手しない。SPEC.md のアプローチ表で選択肢を提示する。

## 関連ドキュメント
- [SPEC.md](SPEC.md) — 技術アプローチの壁打ち
- [TODO.md](TODO.md) — タスク
- [KNOWLEDGE.md](KNOWLEDGE.md) — ハマりポイント
- [README.md](README.md) — 機能概要
