# CLAUDE.md — canmv-k230

## Git / PR ワークフロー

タスク完了時、指示があればPR作成・更新まで一気通貫で実行する。

- **ブランチ**: `feat/`, `docs/`, `style/`, `fix/`, `build/`, `refactor/`, `chore/` prefix。ベースは常に `main`
- **コミット**: conventional commits 形式 `type: short description`
- **`k230_sdk/`**: サブモジュール。変更はコミットしない（`build_sdk.sh` が実行時にパッチする）

### PR作成

```bash
gh pr create --title "type: short description" --body "$(cat <<'EOF'
## Summary
- 変更点を箇条書き

## Test plan
- [x] テスト項目

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

### PR更新

追加コミット & push 後、**必ずPRにコメントを残す**:

```
## type: short description (commit-hash)

変更内容の説明。
```

### PRマージ

```bash
gh pr merge <PR番号> --merge --delete-branch
git remote prune origin
```

## ドキュメント

MkDocs + Material + mkdocs-static-i18n。設定は `mkdocs.yml` 参照。

### 作成手順

1. `docs/ja/` と `docs/en/` に同名 `.md` を作成（日英必須）
2. `mkdocs.yml` の `nav` にエントリ追加（新セクション時は `nav_translations` も）
3. `mkdocs build` で確認

### カテゴリ

- `setup/` — 初期設定、環境構築
- `development/` — サンプルアプリのビルド・実行ガイド
- `technical/` — アーキテクチャ、調査結果

### ソースコード参照

GitHub パーマリンク（コミットハッシュ + 行番号）を使用。ソース変更時はリンクを更新する。

## apps ディレクトリ

各アプリは `apps/<app_name>/` に `CMakeLists.txt` + `src/` で構成。CMake out-of-tree ビルド。

- C: `.c`/`.h`、C++: `.cc`/`.h`
- 新規作成時は既存アプリの `CMakeLists.txt` をテンプレートにする
- コーディングスタイル: Google Style (`clang-format -style=google`)
- cpplint でチェック。SDK 由来で抑制するフィルタ:
  ```
  cpplint --filter=-legal/copyright,-build/include_subdir,-build/namespaces,-build/c++11,-runtime/references,-build/include_order <files>
  ```
  - 上記フィルタ適用後のエラーは **0 件** にすること
  - ヘッダーガードは `APPS_<APP>_SRC_<FILE>_H_` 形式
  - 単一引数コンストラクタには `explicit` を付ける
- `build/` は `.gitignore` で除外済み

### ツールチェーン (`cmake/`)

| ファイル | ターゲット |
|---------|-----------|
| `toolchain-k230-rtsmart.cmake` | bigコア (RT-Smart) — MPP/AI アプリ |
| `toolchain-k230-linux.cmake` | littleコア (Linux) |

### ビルド

```bash
cmake -B apps/<app>/build -S apps/<app> -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-k230-rtsmart.cmake
cmake --build apps/<app>/build
```
