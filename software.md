# museロボット ソフトウェア

PlatformIO（Arduino / ESP32-S3）で顔表示・タッチ入力・状態遷移を動かすファームウェア。

ハードウェア構成は [hardware.md](hardware.md) を参照。

## ビルド

```bash
pio run                 # ビルド
pio run -t upload       # 書き込み
pio device monitor      # シリアルモニタ（115200）
```

ビルド前に `tools/convert_faces.py` が顔アセットを `src/generated/` へ変換する。

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
