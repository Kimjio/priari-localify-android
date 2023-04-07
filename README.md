# priari-localify-android
Localize "Priconne! Grand Masters" for Android client

[한국어](README.ko-KR.md) [日本語](README.ja-JP.md)

## How to use
1. Install [Magisk](https://github.com/topjohnwu/Magisk) v24 or later and enable Zygisk
2. Install module in Magisk
3. Put the `config.json` file in `/sdcard/android/data/jp.co.cygames.priconnegrandmasters/` and the translation file referenced in `dicts`.

## Note
Working directory is `/sdcard/Android/data/jp.co.cygames.priconnegrandmasters/` 

## Config
- `enableLogger` Outputs an unoverwritten string to `data.txt` (`true` / `false`, Default: `false`)
- `maxFps` Max FPS settings (`-1` = Default / `0` = Unlimited / `n > 0` = Limit to n, Default: `-1`)
- `uiAnimationScale` Change UI animation scale (0 < ~, Default: `1`)
  - Caution: Soft lock occurs when set to `0`.
- `uiUseSystemResolution` Renders UI, live subtitles, and more at system resolution. (`true` / `false`, Default: `false`)
- `replaceToCustomFont` Use custom fonts instead of in-game fonts. (`true` / `false`, Default: `false`)
- `fontAssetBundlePath` Font asset bundle path (ex: `custom_font/GyeonggiTitle_Medium/font`)
- `tmproFontAssetName` TextMeshPro Font asset name (ex: `GyeonggiTitle_Medium SDF`)
- `graphicsQuality` Change predefined graphics settings. (`-1` ~ `4`, Default: `-1`)
  - `-1`: Follow the App's choice
  - `0`: `Toon1280`, MSAA OFF
  - `1`: `Toon1280x2`, MSAA x2
  - `2`: `Toon1280x4`, MSAA x4
  - `3`: `ToonFull`, MSAA x8
- `antiAliasing` Change MSAA settings (`-1`, `0`, `2`, `4`, `8`, Default: `-1`)
  - `-1`: Follow the Graphics settings
  - `0`: MSAA OFF
  - `2`: MSAA x2
  - `4`: MSAA x4
  - `8`: MSAA x8
- `replaceAssetsPath` Set the path to the folder containing the assets to replace the downloaded assets in the game.
  - The name of the asset file to be replaced must be the same as the hash name of the original asset file.
  - ex: `2FOXNDZ5H52B3E4JMXVAJ4FRMDE3PX7Q` (Asset containing the Home footer texture and scripts (Android))
- `dicts` Translation file list (List of translation file paths since `/sdcard/Android/data/jp.co.cygames.priconnegrandmasters/`)

## TODO
- Set the app's language to match the system language (currently, fixed to Korean)

## Known issue
- When `maxFps` is set to unlimited, the frame of the game is played from 60 FPS to 30 FPS, which is halved. (Not a typical 30FPS)

## Build
1. Download the source code
2. Use Android Studio to run the gradle task `:module:assembleRelease` to compile, the zip package will be generated in the `out` folder
