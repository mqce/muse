# museロボット ソフトウェア

Muse Gadget SDK（ESP-IDF）上で顔表示・タッチ入力・状態遷移・VOICEVOX 読み上げを動かすファームウェア。

ハードウェア構成は [hardware.md](hardware.md) を参照。

## ディレクトリ構成

| パス | 役割 |
| ---- | ---- |
| `sdk/face/` | 顔タスク、IDF ディスプレイ実装（`display_idf.cpp`）、CMake コンポーネント |
| `sdk/voicevox/` | Muse 返事の VOICEVOX TTS |
| `sdk/patches/` | SDK submodule への結合パッチ（`muse-face.patch`） |
| `src/` | 顔レイヤー合成・アニメ・状態遷移、`display.h` API、生成アセット |
| `face/` | 表情 PNG（`tools/convert_faces.py` が `src/generated/` へ変換） |
| `third_party/muse-gadget-sdk/` | [muse-gadget-sdk](https://github.com/facebookincubator/muse-gadget-sdk) submodule |
| `tools/build.ps1` | Mode 付きエントリポイント |
| `tools/build_muse_sdk.ps1` | パッチ適用・アセット変換・`idf.py` 呼び出し |

タッチ入力はボード API（`muse_board_t::touch_read`）経由。表示は `display.h` の API を `sdk/face/display_idf.cpp` が実装する。

## ビルド

変更内容に応じて Mode を選ぶ（詳細は `.cursor/skills/muse-build/SKILL.md`）。

```powershell
.\tools\build.ps1 -Mode sdk           # 初回・構成変更後（ccache 可）
.\tools\build.ps1 -Mode sdk-app       # 2 回目以降の増分（app 区画）
.\tools\build.ps1 -Mode sdk-flash -Port COMx
```

`build_muse_sdk.ps1` 直呼びも可。

```powershell
.\tools\build_muse_sdk.ps1
.\tools\build_muse_sdk.ps1 -IdfPath D:\esp\esp-idf-v6.0.3
.\tools\build_muse_sdk.ps1 -Target app-flash -Port COMx
```

ビルド時に次を行う。

1. `sdk/patches/muse-face.patch` を submodule へ適用（未適用時）
2. `tools/convert_faces.py` で `face/*.png` → `src/generated/`
3. `sdk/voicevox/sdkconfig.defaults` を生成済み `sdkconfig` へ反映
4. `idf.py`（対象ボード Waveshare 1.75、ビルド dir `build-muse-waveshare-s3-175`）

| 項目 | 値 |
| ---- | -- |
| 対象ボード | Waveshare ESP32-S3-Touch-AMOLED-1.75（プロファイル `s3n` / `waveshare-s3-175`） |
| 必要 IDF | **v6.0.3**（例: `C:\esp\esp-idf-v6.0.3`） |
| 成果物 | `third_party/muse-gadget-sdk/esp32/build-muse-waveshare-s3-175/muse-gadget.bin` |

### 初回セットアップ（Windows）

```powershell
git submodule update --init --recursive

# ESP-IDF v6.0.3（未導入の場合）
git clone -b v6.0.3 --depth 1 --recurse-submodules --shallow-submodules https://github.com/espressif/esp-idf.git C:\esp\esp-idf-v6.0.3
cmd /c "cd /d C:\esp\esp-idf-v6.0.3 && install.bat esp32s3"
```

SDK トークン未設定でもビルドは通る。実機ペアリング前に [gadgets.muse.ai](https://gadgets.muse.ai/settings/sdk-tokens) のトークンを `CONFIG_GADGET_SDK_TOKEN` へ設定する（`idf.py menuconfig`、または書き込み後にシリアルで `>sdk_token=mgst_…`）。

### 書き込み（参考）

```powershell
.\tools\build.ps1 -Mode sdk-flash -Port COMx
# または:
cmd /c "call C:\esp\esp-idf-v6.0.3\export.bat && cd /d third_party\muse-gadget-sdk\esp32 && idf.py -B build-muse-waveshare-s3-175 -p COMx flash monitor"
```

## 状態遷移

放置タイムアウトはいずれも **1分**。タッチで通常に戻る。

```
通常
  └─ 1分放置 → 眠い
                  ├─ 1分放置 → 寝ている
                  │               ├─ 1分放置 → 眠い
                  │               └─ タッチ → 通常
                  └─ タッチ → 通常
```

| 状態 | 遷移条件 |
| ---- | -------- |
| 通常 | 1分放置で眠い |
| 眠い | 1分放置で寝る／タッチで通常 |
| 寝ている | 1分放置で眠い／タッチで通常 |

Muse が listening / thinking / speaking のときはローカル状態より優先し、口パク（しゃべっている）を出す。

## 顔（表情レイヤー）

表情は目・口のレイヤー組み合わせ。アニメーションは目または口だけを差し替える。

| 状態 | 基本表情 | アニメーション |
| ---- | -------- | -------------- |
| 通常 | `eyes_default`, `mouth_default` | まばたき：`eyes_default` → `eyes_half` → `eyes_closed` |
| 眠い | `eyes_half`, `mouth_open1` | うとうと：`eyes_half` → `eyes_closed` |
| 寝ている | `eyes_closed`, `mouth_default` | なし |
| しゃべっている | `eyes_default`, `mouth_open1` | 口パク：`mouth_open1` → `mouth_open2` |

## 返事の読み上げ（VOICEVOX）

Muse からの返事はテキストなので、LAN 上の VOICEVOX エンジンで音声にしてスピーカーから出す。URL、話者 ID、タイムアウトは `sdk/voicevox/sdkconfig.defaults` に書く。いまの値は `http://192.168.1.100:50021`、話者 ID 10（雨晴はう・ノーマル）。

エンジンは ESP32 から届くように起動する（`--host 0.0.0.0` など）。Windows ファイアウォールで TCP 50021 を許可し、別端末から `http://192.168.1.100:50021/version` が開けばよい。届かないときはキャプション表示に戻る。通信は家庭内 LAN の平文 HTTP。
