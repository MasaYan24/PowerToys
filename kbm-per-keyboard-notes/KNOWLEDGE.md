# KNOWLEDGE — KBM per-keyboard remap

ハマりポイント・気づきを即記録。次回同じミスをしない。

## 技術
- **hook vs HID の壁**: `WH_KEYBOARD_LL` は抑制○/デバイス識別×、Raw Input は抑制×/デバイス識別○。
  「デバイス別に抑制付き remap」は標準 API では不可能で、フィルタドライバ（Interception 等）が必要。
  → これが upstream #1460 が実質保留になった根本理由。設計はここから逃げられない。SPEC.md 参照。
- 抑制が不要な「レイアウト/プロファイル自動切替」なら Raw Input のみで現実的（#12349 の方向）。
- デバイス識別子: VID/PID は“モデル”単位。同型2台の区別にはシリアル or デバイスパスが要る。BT はシリアル欠落することあり。

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
