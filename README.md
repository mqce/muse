# muse

全高約10cm、座り姿勢のmuse型デスクトップロボット。

顔ボード（ESP32-S3）で制御・表示・音声を担当し、首3軸・手2軸でしぐさを表現する。第1弾はカメラ無し。

## ドキュメント

| 文書 | 内容 |
| ---- | ---- |
| [hardware.md](hardware.md) | ボード、機構、電源・配線、ピン割当て |
| [software.md](software.md) | 状態遷移、表情、ビルド・構成 |

## 構成の概要

- **制御・表示・音声**：Waveshare ESP32-S3-Touch-AMOLED-1.75
- **首**：ピッチ／ロール／ヨーの3軸（Feetech SCS0009 ×3）
- **手**：左右各1軸のパタパタ
- **入力**：頭タッチセンサー
- **電源**：USB 5V給電

## 開発クイックスタート

PlatformIO でビルド／書き込みする。

```bash
pio run
pio run -t upload
pio device monitor
```

詳細は [software.md](software.md) を参照。
