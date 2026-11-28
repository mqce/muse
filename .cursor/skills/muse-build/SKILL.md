---
name: muse-build
description: >-
  Selects the muse firmware build/flash path for Muse Gadget SDK / ESP-IDF.
  Use when building, flashing, uploading, verifying on device, running idf.py,
  or when the user mentions ビルド, 書き込み, フラッシュ, 動作確認, build_muse_sdk, or ccache.
---

# muse ビルド選択

Muse Gadget SDK（ESP-IDF）のみ。変更内容に応じて Mode を選ぶ。

## 手順

1. 変更ファイル（またはこれから触る範囲）を分類する
2. 下表の **Mode** を決める（迷ったら `sdk`）
3. 実行前にユーザーへ Mode を一言伝える
4. `.\tools\build.ps1 -Mode <mode> ...` を実行する（Windows / PowerShell）

## Mode 判定

| Mode | いつ使う | 典型パス |
|------|----------|----------|
| `sdk` | 初回・クリーン後・CMake/`sdkconfig`/コンポーネント構成の変更 | `sdk/**`, `src/**`, `face/**`, `voice/**`, `third_party/muse-gadget-sdk/**`, `tools/**` |
| `sdk-app` | 2 回目以降の増分（app 区画のみ） | 同上 |
| `flash` | **既存バイナリの書き込みのみ（ビルドしない）** | 要 `-Port COMx` |
| `sdk-flash` | `flash` の別名（互換） | 要 `-Port COMx` |

### 判定ルール

- 通常のコード変更 → **`sdk-app`**
- 初回・クリーン後・CMake/`sdkconfig`/EXTRA_COMPONENT 構成の変更後 → **`sdk`**
- **「書き込んでください」「フラッシュして」など書き込みだけの依頼 → 必ず `flash`（ビルドしない）**
- ビルドしてから書き込む明示依頼（「ビルドして書いて」等）のときだけ、先に `sdk` / `sdk-app` してから `flash`
- `fullclean` / ビルド dir 削除 / `sdkconfig` 作り直しは **ユーザーが明示したときだけ**

## コマンド

```powershell
.\tools\build.ps1 -Mode sdk
.\tools\build.ps1 -Mode sdk-app
.\tools\build.ps1 -Mode flash -Port COM5
.\tools\build.ps1 -Mode sdk -IdfPath D:\esp\esp-idf-v6.0.3
.\tools\build.ps1 -Mode sdk -NoCcache
```

- `flash` は `build-muse-waveshare-s3-175/muse-gadget.bin` を esptool で書くだけ。アセット変換・パッチ・コンパイルはしない
- デフォルトで **ccache 有効**（ビルド Mode のみ。未インストールなら警告して通常ビルド）
- シリアルモニタはユーザーが求めたときだけ `idf.py monitor`

## やってはいけないこと

- 「書き込んで」と言われたのに `sdk` / `sdk-app` / `app-flash` でビルドし直すこと
- 理由なく `idf.py fullclean` やビルドディレクトリ削除
- Mode を伝えずに重いフルビルドを開始する

## 補足

- ESP-IDF v6.0.3
- 成果物: `third_party/muse-gadget-sdk/esp32/build-muse-waveshare-s3-175/muse-gadget.bin`
- 顔ロジック `src/`、アセット `face/` / `voice/`、結合 `sdk/face` + `sdk/patches`
- 詳細: [software.md](../../../software.md)
