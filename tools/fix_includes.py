"""ボード variant の pins_arduino.h を確実に include path へ入れる。"""

Import("env")  # type: ignore[name-defined]  # noqa: F821

from os.path import join

framework = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
if framework:
    variant = env.BoardConfig().get("build.variant", "esp32s3")
    env.Append(CPPPATH=[join(framework, "variants", variant)])
