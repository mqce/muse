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
| `sdk` | 初回・クリーン後・CMake/`sdkconfig`/コンポーネント構成の変更 | `sdk/**`, `src/**`, `face/**`, `third_party/muse-gadget-sdk/**`, `tools/**` |
| `sdk-app` | 2 回目以降の増分（app 区画のみ） | 同上 |
| `sdk-flash` | 実機へ app 書き込み | 要 `-Port COMx` |

### 判定ルール

- 通常のコード変更 → **`sdk-app`**
- 初回・クリーン後・CMake/`sdkconfig`/EXTRA_COMPONENT 構成の変更後 → **`sdk`**
- `fullclean` / ビルド dir 削除 / `sdkconfig` 作り直しは **ユーザーが明示したときだけ**

## コマンド

```powershell
.\tools\build.ps1 -Mode sdk
.\tools\build.ps1 -Mode sdk-app
.\tools\build.ps1 -Mode sdk-flash -Port COM5
.\tools\build.ps1 -Mode sdk -IdfPath D:\esp\esp-idf-v6.0.3
.\tools\build.ps1 -Mode sdk -NoCcache
```

- デフォルトで **ccache 有効**（未インストールなら警告して通常ビルド）
- シリアルモニタはビルド成功後、ユーザーが求めたときだけ `idf.py monitor`

## やってはいけないこと

- 理由なく `idf.py fullclean` やビルドディレクトリ削除
- Mode を伝えずに重いフルビルドを開始する

## 補足

- ESP-IDF v6.0.3
- 成果物: `third_party/muse-gadget-sdk/esp32/build-muse-waveshare-s3-175/muse-gadget.bin`
- 顔ロジック `src/`、アセット `face/`、結合 `sdk/face` + `sdk/patches`
- 詳細: [software.md](../../../software.md)
