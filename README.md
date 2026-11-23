# muse

全高約10cm、座り姿勢のmuse型デスクトップロボット。

顔ボード（ESP32-S3）で制御・表示・音声を担当し、首3軸・手2軸でしぐさを表現する。第1弾はカメラ無し。

## ドキュメント

| 文書 | 内容 |
| ---- | ---- |
| [hardware.md](hardware.md) | ボード、機構、電源・配線、ピン割当て |
| [software.md](software.md) | 状態遷移、表情、ビルド・構成（Muse Gadget SDK / ESP-IDF） |

## 構成の概要

- **制御・表示・音声**：Waveshare ESP32-S3-Touch-AMOLED-1.75
- **首**：ピッチ／ロール／ヨーの3軸（Feetech SCS0009 ×3）
- **手**：左右各1軸のパタパタ
- **入力**：頭タッチセンサー
- **電源**：USB 5V給電

## リポジトリ構成

| パス | 内容 |
| ---- | ---- |
| `sdk/` | Muse Gadget SDK への結合（顔タスク、VOICEVOX、パッチ） |
| `src/` | 顔描画・アニメ・状態遷移（SDK コンポーネントから参照） |
| `face/` | 表情 PNG アセット |
| `third_party/muse-gadget-sdk/` | Muse Gadget SDK（git submodule） |
| `tools/` | ビルド・顔アセット変換スクリプト |

## 開発クイックスタート

Muse Gadget SDK（ESP-IDF **v6.0.3**）でビルドする。

```powershell
git submodule update --init --recursive
.\tools\build.ps1 -Mode sdk
.\tools\build.ps1 -Mode sdk-flash -Port COMx
```

2 回目以降の増分は `-Mode sdk-app`。Mode の判定は `.cursor/skills/muse-build/SKILL.md`。詳細は [software.md](software.md)。
