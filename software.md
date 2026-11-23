# museロボット ソフトウェア

PlatformIO（Arduino / ESP32-S3）で顔表示・タッチ入力・状態遷移を動かすファームウェア。

ハードウェア構成は [hardware.md](hardware.md) を参照。

## ビルド

変更内容に応じて Mode を選ぶ（詳細は `.cursor/skills/muse-build/SKILL.md`）。

```powershell
.\tools\build.ps1 -Mode face          # 顔・状態のみ（PlatformIO）
.\tools\build.ps1 -Mode face-upload   # 上記を実機へ
.\tools\build.ps1 -Mode sdk           # Muse Gadget SDK（増分・ccache）
.\tools\build.ps1 -Mode sdk-app       # SDK の app 区画のみ
.\tools\build.ps1 -Mode sdk-flash -Port COMx
```

従来どおり個別コマンドも使える。

```bash
pio run                 # ビルド
pio run -t upload       # 書き込み
pio device monitor      # シリアルモニタ（115200）
```

ビルド前に `tools/convert_faces.py` が顔アセットを `src/generated/` へ変換する（SDK 経路ではスクリプト内で実行）。

## 主なモジュール

| モジュール | 役割 |
| ---------- | ---- |
| `main.cpp` | 初期化・ループ、状態と表情の適用 |
| `display` | AMOLED 初期化・描画 |
| `face_renderer` / `face_layer` | 背景＋目＋口のレイヤー合成 |
| `face_anim` | まばたき・うとうと等のアニメーション |
| `muse_state` | 通常／眠い／寝ているの状態遷移 |
| `touch` | 頭タッチ入力 |

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

## 顔（表情レイヤー）

表情は目・口のレイヤー組み合わせ。アニメーションは目または口だけを差し替える。

| 状態 | 基本表情 | アニメーション |
| ---- | -------- | -------------- |
| 通常 | `eyes_default`, `mouth_default` | まばたき：`eyes_default` → `eyes_half` → `eyes_closed` |
| 眠い | `eyes_half`, `mouth_open1` | うとうと：`eyes_half` → `eyes_closed` |
| 寝ている | `eyes_closed`, `mouth_default` | なし |
| しゃべっている | `eyes_default`, `mouth_open1` | 口パク：`mouth_open1` → `mouth_open2` |

## 依存ライブラリ

`platformio.ini` より：

- [GFX Library for Arduino](https://github.com/moononournation/Arduino_GFX)
- [SensorLib](https://github.com/lewisxhe/SensorLib)

## Muse Gadget SDK

[facebookincubator/muse-gadget-sdk](https://github.com/facebookincubator/muse-gadget-sdk) を `third_party/muse-gadget-sdk` に submodule で導入している。ESP-IDF ファームであり、現行の PlatformIO 顔ファームとは別系統。

| 項目 | 値 |
| ---- | -- |
| 対象ボード | Waveshare ESP32-S3-Touch-AMOLED-1.75（SDK プロファイル `s3n` / `waveshare-s3-175`） |
| 必要 IDF | **v6.0.3**（例: `C:\esp\esp-idf-v6.0.3`） |
| 成果物 | `third_party/muse-gadget-sdk/esp32/build-muse-waveshare-s3-175/muse-gadget.bin` |

### 初回セットアップ（Windows）

```powershell
git submodule update --init --recursive

# ESP-IDF v6.0.3（未導入の場合）
git clone -b v6.0.3 --depth 1 --recurse-submodules --shallow-submodules https://github.com/espressif/esp-idf.git C:\esp\esp-idf-v6.0.3
cmd /c "cd /d C:\esp\esp-idf-v6.0.3 && install.bat esp32s3"
```

### ビルド

```powershell
.\tools\build_muse_sdk.ps1
# 別パスの IDF を使う場合:
.\tools\build_muse_sdk.ps1 -IdfPath D:\esp\esp-idf-v6.0.3
```

SDK トークン未設定でもビルドは通るが、実機ペアリング前に [gadgets.muse.ai](https://gadgets.muse.ai/settings/sdk-tokens) のトークンを `CONFIG_GADGET_SDK_TOKEN` へ設定する（`idf.py menuconfig`、または書き込み後にシリアルで `>sdk_token=mgst_…`）。

### 返事の読み上げ（VOICEVOX）

Muse からの返事はテキストなので、LAN 上の VOICEVOX エンジンで音声にしてスピーカーから出す。URL、話者 ID、タイムアウトは `sdk/voicevox/sdkconfig.defaults` に書く。ビルドスクリプトが生成済みの `sdkconfig` へこの内容を上書きしてからコンパイルする。いまの値は `http://192.168.1.100:50021`、話者 ID 10（雨晴はう・ノーマル）。エンジンは localhost だけではなく ESP32 から届くように起動する（`--host 0.0.0.0` など）。Windows ファイアウォールで TCP 50021 を許可し、別の端末から `http://192.168.1.100:50021/version` が開ければよい。届かないときはキャプションだけの表示に戻る。通信は家庭内 LAN の平文 HTTP。

### 書き込み（参考）

```powershell
cmd /c "call C:\esp\esp-idf-v6.0.3\export.bat && cd /d third_party\muse-gadget-sdk\esp32 && idf.py -B build-muse-waveshare-s3-175 -p COMx flash monitor"
```
