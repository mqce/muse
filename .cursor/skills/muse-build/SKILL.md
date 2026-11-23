---
name: muse-build
description: >-
  Selects the fastest muse firmware build/flash path from the changed files
  (PlatformIO face firmware vs Muse Gadget SDK / ESP-IDF). Use when building,
  flashing, uploading, verifying on device, running pio/idf.py, or when the
  user mentions ビルド, 書き込み, フラッシュ, 動作確認, build_muse_sdk, or ccache.
---

# muse ビルド選択

変更内容に応じて **最速の経路** を選ぶ。フル SDK をデフォルトにしない。

## 手順

1. 変更ファイル（またはこれから触る範囲）を分類する
2. 下表の **Mode** を決める（迷ったら `face`）
3. 実行前にユーザーへ Mode を一言伝える
4. `.\tools\build.ps1 -Mode <mode> ...` を実行する（Windows / PowerShell）

## Mode 判定

| Mode | いつ使う | 典型パス |
|------|----------|----------|
| `face` | 顔・表情・状態・表示のみ（Muse 通信不要） | `src/**`, `assets/**`, `platformio.ini`, `tools/convert_faces.py`, `tools/fix_includes.py` |
| `face-upload` | 上記を実機で確認 | 同上＋書き込み |
| `sdk` | Muse 連携・SDK パッチ・VOICEVOX 結合・`sdk/` の IDF コンポーネント | `sdk/**`, `third_party/muse-gadget-sdk/**`, `tools/build_muse_sdk.ps1` |
| `sdk-app` | SDK 増分ビルドのみ（app 区画）。クリーン直後や初回以外 | 同上 |
| `sdk-flash` | SDK を実機へ（app のみ書き込み） | 要 `-Port COMx` |

### 判定ルール

- `src/` と `sdk/face` で共有する顔ロジックだけ → **`face`**（PlatformIO）。SDK は結合時まで触らない
- `sdk/voicevox` や Muse セッション／ボード／パッチ → **`sdk`** / **`sdk-app`**
- `src/` と `sdk/` を同時変更し Muse 実機確認が必要 → **`sdk`**（顔アセット変換はスクリプト内で実行される）
- `fullclean` / ビルド dir 削除 / `sdkconfig` 作り直しは **ユーザーが明示したときだけ**
- 2 回目以降の SDK は原則 **`sdk-app`**（増分）。初回・クリーン後・CMake/コンフィグ大変更後は **`sdk`**

## コマンド

```powershell
.\tools\build.ps1 -Mode face
.\tools\build.ps1 -Mode face-upload
.\tools\build.ps1 -Mode sdk
.\tools\build.ps1 -Mode sdk-app
.\tools\build.ps1 -Mode sdk-flash -Port COM5
# 任意:
.\tools\build.ps1 -Mode sdk -IdfPath D:\esp\esp-idf-v6.0.3
.\tools\build.ps1 -Mode sdk -NoCcache   # ccache 無効化
```

- SDK 系はデフォルトで **ccache 有効**（未インストールなら警告して通常ビルド）
- シリアルモニタはビルド成功後、ユーザーが求めたときだけ `pio device monitor` または `idf.py monitor`

## やってはいけないこと

- 顔だけの変更で毎回 `.\tools\build_muse_sdk.ps1` / フル `idf.py build`
- 理由なく `idf.py fullclean` やビルドディレクトリ削除
- Mode を伝えずに重い SDK ビルドを開始する

## 補足

- PlatformIO: `platformio.ini` / Arduino 顔ファーム
- SDK: ESP-IDF v6.0.3、成果物 `third_party/muse-gadget-sdk/esp32/build-muse-waveshare-s3-175/muse-gadget.bin`
- 詳細ドキュメント: [software.md](../../../software.md)
