# museロボット ソフトウェア

Muse Gadget SDK（ESP-IDF）上で顔表示・タッチ入力・状態遷移・VOICEVOX 読み上げを動かすファームウェア。

ハードウェア構成は [hardware.md](hardware.md) を参照。状態遷移の正本は [spec/state-transition-model-v1.md](spec/state-transition-model-v1.md)。

## ディレクトリ構成

| パス | 役割 |
| ---- | ---- |
| `sdk/face/` | 顔タスク、IDF ディスプレイ実装（`display_idf.cpp`）、CMake コンポーネント |
| `sdk/voicevox/` | Muse 返事の VOICEVOX TTS |
| `sdk/patches/` | SDK submodule への結合パッチ（`muse-face.patch`） |
| `src/` | 顔レイヤー合成・アニメ・ムード状態遷移、固定クリップ再生 API、生成アセット |
| `face/` | 表情 PNG（`tools/convert_faces.py` が `src/generated/` へ変換） |
| `voice/` | 固定セリフ WAV（`tools/convert_voice.py` が 16 kHz PCM へ変換） |
| `spec/` | 状態遷移モデルと固定セリフ一覧 |
| `third_party/muse-gadget-sdk/` | [muse-gadget-sdk](https://github.com/facebookincubator/muse-gadget-sdk) submodule |
| `tools/build.ps1` | Mode 付きエントリポイント |
| `tools/build_muse_sdk.ps1` | パッチ適用・アセット変換・`idf.py` 呼び出し |

タッチ入力はボード API（`muse_board_t::touch_read`）経由。表示は `display.h` の API を `sdk/face/display_idf.cpp` が実装する。

## ビルド

変更内容に応じて Mode を選ぶ（詳細は `.cursor/skills/muse-build/SKILL.md`）。

```powershell
.\tools\build.ps1 -Mode sdk           # 初回・構成変更後（ccache 可）
.\tools\build.ps1 -Mode sdk-app       # 2 回目以降の増分（app 区画）
.\tools\build.ps1 -Mode flash -Port COMx   # 既存 bin の書き込みのみ（ビルドしない）
```

`build_muse_sdk.ps1` 直呼びも可。

```powershell
.\tools\build_muse_sdk.ps1
.\tools\build_muse_sdk.ps1 -IdfPath D:\esp\esp-idf-v6.0.3
.\tools\build_muse_sdk.ps1 -Target flash -Port COMx
```

ビルド時に次を行う。

1. `sdk/patches/muse-face.patch` を submodule へ適用（未適用時）
2. `tools/convert_faces.py` で `face/*.png` → `src/generated/`
3. `tools/convert_voice.py` で `voice/*.wav` → `src/generated/`
4. `sdk/voicevox/sdkconfig.defaults` を生成済み `sdkconfig` へ反映
5. `idf.py`（対象ボード Waveshare 1.75、ビルド dir `build-muse-waveshare-s3-175`）

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
.\tools\build.ps1 -Mode flash -Port COMx
```

## 状態遷移

SDK のモード（BOOT / IDLE / LISTENING / THINKING / SPEAKING / ERROR / OFF）を最上位とし、IDLE 配下にムードのサブステートを持つ。詳細・数値は [spec/state-transition-model-v1.md](spec/state-transition-model-v1.md) と [spec/state_model.yaml](spec/state_model.yaml)。

パラメータ（RAM のみ）:

| 名前 | 範囲 | 初期値 |
| ---- | ---- | ------ |
| 幸福度 | 0〜10 | 5 |
| ねむけ | 0〜100 | 30 |
| たいくつ | 0〜100 | 20 |

IDLE サブステート:

| 状態 | 主な入り方 |
| ---- | ---------- |
| 通常 | 初期／回復時の再判定 |
| 幸福 | 幸福度 ≥ 8（出口は ≤ 5） |
| すねてる | 幸福度 = 0 が 30 分継続 |
| ねむい | ねむけ ≥ 70 |
| ねてる | ねむけ ≥ 90（起床は ≤ 20） |
| 接続エラー | Link / Wi-Fi / Muse 到達の致命状態が 5 秒継続（ねてる間は起きてから判定） |

優先度: `OFF > BOOT > ERROR > LISTENING / THINKING / SPEAKING > IDLE`。会話終了時はパラメータからサブステートを再判定する。

タッチ（画面）:

- 短押し → 顔タッチ（たいくつ −10、ねむいときねむけ −10）
- 500ms ホールド、または 1.5 秒以内に 3 回タップ → なで（幸福度 +1、たいくつ −20）
- なで／タッチの瞬間、目 `eyes_squeeze`・口 M1 を約 1.2 秒重ねる

## 顔（表情レイヤー）

レイヤー順は bg → 目 → 口。パーツ名は `face/` の PNG stem。

| 状態 | 基本表情 | アニメ |
| ---- | -------- | ------ |
| 通常 | E1 + M1 | まばたき |
| 幸福 | E4 + M4 | なし |
| すねてる | E2 + `mouth_frown` | なし |
| ねむい | ねむけに応じ E1→E2→E3 + M1 | E1 のあいだは通常と同じまばたき |
| ねてる | E3 + M1 | なし |
| 接続エラー / ERROR | `eyes_squeeze` + `mouth_frown` | なし |
| LISTENING | E3 + M1 | 時々開眼 |
| THINKING | 上目 + `mouth_flat` | キョロキョロ |
| SPEAKING | E1 + 口パク M2↔M3 | 150ms 交代 |
| BOOT / OFF | E3 + M1 | なし |

たいくつ ≥ 70 かつ通常／幸福のとき、時々上目でキョロキョロ（一人遊び）。手パタパタはサーボ未配線のためログのみ。

## 固定セリフ（デバイス内 WAV）

[voice/](voice/) の事前合成 WAV を 16 kHz に変換して埋め込む。TTS サーバー不要。

| クリップ | 発動 |
| -------- | ---- |
| `wake_up` | ねてる → 通常 |
| `happy` | 幸福へ入ったとき |
| `thinking` | THINKING に入ったとき 1 回（返事音声が来たら途中で止める） |
| `call` | 通常かつたいくつ ≥ 80 が 1 分継続（以後 10 分間隔） |
| `touch` | 顔の短タッチ（「わぁ」）1 回 |
| `giggle` / `ehehe` / `happy` / 無音 | なでたとき 1 回。無音 50%、ふふ 20%、えへへ 20%、happy 10% |

詳細は [spec/fixed-voice-lines.md](spec/fixed-voice-lines.md)。

## 返事の読み上げ（VOICEVOX）

Muse からの返事はテキストなので、LAN 上の VOICEVOX エンジンで音声にしてスピーカーから出す。URL、話者 ID、タイムアウトは `sdk/voicevox/sdkconfig.defaults` に書く。いまの値は `http://192.168.1.100:50021`、話者 ID 10（雨晴はう・ノーマル）。

エンジンは ESP32 から届くように起動する（`--host 0.0.0.0` など）。Windows ファイアウォールで TCP 50021 を許可し、別端末から `http://192.168.1.100:50021/version` が開けばよい。届かないときはキャプション表示に戻る。通信は家庭内 LAN の平文 HTTP。
