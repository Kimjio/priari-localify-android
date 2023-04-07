# priari-localify-android
"프리코네! 그랜드 마스터즈" 안드로이드 클라이언트 현지화

[English](README.md) [日本語](README.ja-JP.md)

## 사용법
1. v24 이상의 [Magisk](https://github.com/topjohnwu/Magisk) 설치 후 Zygisk 를 활성화합니다.
2. Magisk 에서 모듈을 설치합니다.
3. `/sdcard/Android/data/jp.co.cygames.priconnegrandmasters/` 에 `config.json` 파일, `dicts` 에서 참조하는 번역 파일을 넣습니다.

## 참고사항
작업 경로는 `/sdcard/Android/data/jp.co.cygames.priconnegrandmasters/` 입니다.

## 설정
- `enableLogger` 덮어쓰지 않은 문자열을 `data.txt` 에 출력합니다. (`true` / `false`, 기본값: `false`)
- `maxFps` 최대 프레임 설정 (`-1` = 기본값 / `0` = 무제한 / `n > 0` = n까지 제한, 기본값: `-1`)
- `uiAnimationScale` UI 애니메이션 배율 변경 (0 < ~, 기본값: `1`)
  - 주의: `0` 으로 설정 시 소프트락이 발생합니다.
- `uiUseSystemResolution` UI, 라이브 자막 등을 시스템 해상도로 렌더링합니다. (`true` / `false`, 기본값: `false`)
- `replaceToCustomFont` 게임 내 폰트를 사용하지 않고 사용자 지정 폰트를 사용합니다. (`true` / `false`, 기본값: `false`)
- `fontAssetBundlePath` 폰트 애셋 번들 경로 (예: `custom_font/GyeonggiTitle_Medium/font`)
- `tmproFontAssetName` TextMeshPro 폰트 애셋 이름 (예: `GyeonggiTitle_Medium SDF`)
- `graphicsQuality` 사전 정의된 그래픽 설정을 변경합니다. (`-1` ~ `4`, 기본값: `-1`)
  - `-1`: 앱에서 결정
  - `0`: `Toon1280`, MSAA OFF
  - `1`: `Toon1280x2`, MSAA x2
  - `2`: `Toon1280x4`, MSAA x4
  - `3`: `ToonFull`, MSAA x8
- `antiAliasing` MSAA 설정을 변경합니다. (`-1`, `0`, `2`, `4`, `8`, 기본값: `-1`)
  - `-1`: 그래픽 설정을 따름
  - `0`: MSAA OFF
  - `2`: MSAA x2
  - `4`: MSAA x4
  - `8`: MSAA x8
- `replaceAssetsPath` 게임에서 다운로드한 애셋을 대체할 애셋들이 담긴 폴더의 경로를 설정합니다.
  - 대체할 애셋 파일의 이름은 원본 애셋 파일의 해시명과 동일해야 합니다.
  - 예: `2FOXNDZ5H52B3E4JMXVAJ4FRMDE3PX7Q` (홈 Footer 텍스처 및 스크립트가 포함된 애셋 (Android))
- `dicts` 번역 파일 목록 (`/sdcard/Android/data/jp.co.cygames.priconnegrandmasters/` 이후의 번역 파일 경로 목록)

## TODO
- 앱의 언어가 시스템 언어와 일치하도록 설정 (현재, 한국어 고정)

## 알려진 문제
- `maxFps` 를 무제한으로 설정 시 게임의 프레임이 60FPS 에서 반토막난 30FPS 으로 재생됩니다.

## 빌드
1. 소스 코드를 다운 받습니다.
2. Android Studio 사용하여 gradle 작업 `:module:assembleRelease` 을 실행해서 컴파일하면, `out` 폴더에 zip 패키지가 생성됩니다.
