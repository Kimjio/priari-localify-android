# priari-localify-android
「プリコネ！グランドマスターズ」をAndroidクライアント向けにローカライズ

[English](README.md) [한국어](README.ko-KR.md)

## 使用方法
1. [Magisk](https://github.com/topjohnwu/Magisk) v24またはそれ以降をインストールし、Zygiskを有効化してください。
2. Magiskにpriari-localify-androidのモジュールをインストールしてください。
3. `config.json` ファイルを `/sdcard/android/data/jp.co.cygames.priconnegrandmasters/` `dicts`に参照されている翻訳ファイルを配置します。

## 備考
ディレクトリは `/sdcard/Android/data/jp.co.cygames.priconnegrandmasters/` になります。

## 設定
- `enableLogger` 書き換えられていない文字列を`data.txt`に出力します。 (`true` / `false` デフォルト: `false`)
- `maxFps` 最大のFPS値を設定します。 (`-1` = デフォルト / `0` = 無制限 / `n > 0` = 指定した値に制限 デフォルト: `-1`)
- `uiAnimationScale` UIアニメーションスケールの変更をします。 (0 < ~ デフォルト: `1`)
  - 注意: `0`に設定をするとソフトロックがかかります。
- `uiUseSystemResolution` システムの解像度でUIをレンダリングします。 (`true` / `false` デフォルト: `false`)
- `replaceToCustomFont` ゲーム内のフォントをカスタムフォントに変更します。 (`true` / `false` デフォルト: `false`)
- `fontAssetBundlePath` フォントアセットバンドルパスを指定します。 (例: `custom_font/GyeonggiTitle_Medium/font`)
- `tmproFontAssetName` TextMeshProフォントアセット名 (例: `GyeonggiTitle_Medium SDF`)
- `graphicsQuality` あらかじめ設定されているグラフィック設定を変更します。 (`-1` ~ `4` デフォルト: `-1`)
  - `-1`: アプリの設定に従う
  - `0`: `Toon1280`、アンチエイリアシング オフ
  - `1`: `Toon1280x2`、アンチエイリアシング x2
  - `2`: `Toon1280x4`、アンチエイリアシング x4
  - `3`: `ToonFull`、アンチエイリアシング x8
- `antiAliasing` アンチエイリアシングの設定を変更します。 (`-1`、 `0`、 `2`、 `4`、 `8` デフォルト: `-1`)
  - `-1`: グラフィックの設定に従う
  - `0`: アンチエイリアシング OFF
  - `2`: アンチエイリアシング x2
  - `4`: アンチエイリアシング x4
  - `8`: アンチエイリアシング x8
- `replaceAssetsPath` ダウンロードをしたアセットをゲーム内で置き換えるためのアセットが入ったフォルダを指定します。
  - 置換をするアセットは元のアセットファイルと同じハッシュ名である必要があります。
  - 例: `2FOXNDZ5H52B3E4JMXVAJ4FRMDE3PX7Q` (ホームフッターのテクスチャとスクリプトを含むアセット (Android))
- `dicts` 翻訳ファイルリスト (翻訳ファイルのパス `/sdcard/Android/data/jp.co.cygames.priconnegrandmasters/`)

## TODO
- アプリの言語がシステム言語と一致するように設定(現在、韓国語固定)

## 既知の問題
- `maxFps`を無制限にするとゲームのフレームレートが60FPSから30FPSになってしまう。 (一般的な30FPSではありません)

## ビルド
1. ソースコードをダウンロードします。
2. Android Studioを使用し、gradle task `:module:assembleRelease`を実行しコンパイルをする事で`out`フォルダ内にzipファイルが生成されます。
