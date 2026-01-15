# 1.0.0 (2026-01-15)


### Bug Fixes

* added missing files ([a52b5ba](https://github.com/tlamy/P2Pro-Viewer/commit/a52b5ba6de63dc55c9d1b25cc8fdaeefb440a045))
* added missing pre-rendered toolbar icons ([36ff5a1](https://github.com/tlamy/P2Pro-Viewer/commit/36ff5a1c79e3dc8f736b930ad8cf1deedae61af3))
* better initial window sizing on hidpi screens ([673cd97](https://github.com/tlamy/P2Pro-Viewer/commit/673cd9753f3df791677f9c6b7b24370e55156e1c))
* fixed too much hidpi scaling ([df1cbbb](https://github.com/tlamy/P2Pro-Viewer/commit/df1cbbb8642ddfe05c942a12c481183d49782794))
* improved frame acquisition stability on Linux by adding retries and timeout with `poll` ([1ca839b](https://github.com/tlamy/P2Pro-Viewer/commit/1ca839bec847b7fe0a00085205ec6e059f912541))


### Features

* added CI/CD workflow and semantic-release for automated builds and releases ([0f932f0](https://github.com/tlamy/P2Pro-Viewer/commit/0f932f0753e158ad5d675d1d3e2d4d2cf7df13c9))
* added CI/CD workflow and semantic-release for automated builds and releases ([be97fd8](https://github.com/tlamy/P2Pro-Viewer/commit/be97fd8e67c33b8cbe73108979b522fe0abfac64))
* added icon generation tools and macOS app bundling improvements ([05ce19a](https://github.com/tlamy/P2Pro-Viewer/commit/05ce19a01bb2e00d7da30962aa537f2657d134be))
* added Linux support ([a0b5053](https://github.com/tlamy/P2Pro-Viewer/commit/a0b5053faac3c7bae48c6895df39095f95775605))
* added macOS app bundling script and improved portability ([611ac19](https://github.com/tlamy/P2Pro-Viewer/commit/611ac1928973157c5d0937580c60dfd41af44847))
* added rotation support and new toolbar to CameraWindow ([569a833](https://github.com/tlamy/P2Pro-Viewer/commit/569a833bb5c73be9b074fffe79d0a590c1b5772f))
* added scaling support and icon-based enhancements to CameraWindow ([58cbe50](https://github.com/tlamy/P2Pro-Viewer/commit/58cbe5073f0d29d45d52fb87cf0c3d40cc6453d3))
* added video recording and hotspot detection functionality ([e00644f](https://github.com/tlamy/P2Pro-Viewer/commit/e00644f4f9f28218c378d1de20e4562d3ab6ddb6))
* added window resizing ([745097c](https://github.com/tlamy/P2Pro-Viewer/commit/745097cdbb56a18292ff10f6fa7dfe43e81ee19e))
* added zoom functionality, icon-based toolbar, and improved scaling for CameraWindow ([3152930](https://github.com/tlamy/P2Pro-Viewer/commit/31529302e07586f9605909e0804f919fee6002b6))
* c++/sdl implementation ([04d111b](https://github.com/tlamy/P2Pro-Viewer/commit/04d111bfe0f418757640e4864007d0e06b7c89ad))
* First live picture in macOS ([e0f9592](https://github.com/tlamy/P2Pro-Viewer/commit/e0f95924faac1b56bd44415cbc1fef2e2afe4fdf))
* improved camera reconnection logic and added scanning message when disconnected ([e247865](https://github.com/tlamy/P2Pro-Viewer/commit/e2478659561f7bc291dd87c45b165884bd8d9a7e))
* removed redundant `libusb` dependency on macOS ([b3cc008](https://github.com/tlamy/P2Pro-Viewer/commit/b3cc008bc4e2008c4b4ec0239d8faf9441dc3b00))
* removed the ffmpeg dependency from the macOS version of the project by implementing a native video recording solution using macOS frameworks ([25c7344](https://github.com/tlamy/P2Pro-Viewer/commit/25c73449f673ac0aaec2cdf3ec3b8bc45722f3b7))
* replaced opencv dependency by ffmpeg/v4l2 on Linux and native Apple frameworks on macOS ([2071fe6](https://github.com/tlamy/P2Pro-Viewer/commit/2071fe63707c60fb96e6cbb2f56b7c4faee0b61f))
* replaced opencv dependency by ffmpeg/v4l2 on Linux and native Apple frameworks on macOS ([40f26ea](https://github.com/tlamy/P2Pro-Viewer/commit/40f26eaddffe77836b7e76daa7a86fdf713c6ccc))
* switched to native AVFoundation for video capture on macOS, removed ffmpeg fallback ([7dc7bea](https://github.com/tlamy/P2Pro-Viewer/commit/7dc7beadd06c299127e3e21f915f0779420802b8))
* switched to native AVFoundation for video capture on macOS, removed ffmpeg fallback ([2ad8299](https://github.com/tlamy/P2Pro-Viewer/commit/2ad82996e14e559955c7ec7ce72e174206e2ae9f))
