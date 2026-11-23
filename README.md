# muse

全高約10cm、座り姿勢のmuse型デスクトップロボット。

顔ボード（ESP32-S3）で制御・表示・音声を担当し、首3軸・手2軸でしぐさを表現する。第1弾はカメラ無し。

## ドキュメント

| 文書 | 内容 |
| ---- | ---- |
| [hardware.md](hardware.md) | ボード、機構、電源・配線、ピン割当て |
| [software.md](software.md) | 状態遷移、表情、ビルド・構成（PlatformIO／Muse Gadget SDK） |

## 構成の概要

- **制御・表示・音声**：Waveshare ESP32-S3-Touch-AMOLED-1.75
- **首**：ピッチ／ロール／ヨーの3軸（Feetech SCS0009 ×3）
- **手**：左右各1軸のパタパタ
- **入力**：頭タッチセンサー
- **電源**：USB 5V給電

## 開発クイックスタート

変更内容でビルド経路を分ける（顔だけなら PlatformIO、Muse 連携なら SDK）。エントリポイント:

```powershell
.\tools\build.ps1 -Mode face          # 顔・状態（速い）
.\tools\build.ps1 -Mode face-upload
.\tools\build.ps1 -Mode sdk           # Muse Gadget SDK
.\tools\build.ps1 -Mode sdk-flash -Port COMx
```

判定ルールは `.cursor/skills/muse-build/SKILL.md`。前提の ESP-IDF は **v6.0.3**。詳細は [software.md](software.md)。

初回 SDK だけ:

```powershell
git submodule update --init --recursive
.\tools\build_muse_sdk.ps1
```
