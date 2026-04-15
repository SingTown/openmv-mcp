# Changelog

## [1.4.2](https://github.com/SingTown/openmv-mcp/compare/v1.4.1...v1.4.2) (2026-04-15)


### Bug Fixes

* smoke test skip-github-release pipeline ([94e985c](https://github.com/SingTown/openmv-mcp/commit/94e985cc052d159326f9bac922e97334598eb1db))

## [1.4.1](https://github.com/SingTown/openmv-mcp/compare/v1.4.0...v1.4.1) (2026-04-15)


### Bug Fixes

* trigger release workflow smoke test ([0fc3d9d](https://github.com/SingTown/openmv-mcp/commit/0fc3d9d6f2144ec13c62d248d5ff2cac6c2c9fc9))

## [1.4.0](https://github.com/SingTown/openmv-mcp/compare/v1.3.0...v1.4.0) (2026-04-15)


### Features

* emit MCP progress notifications for firmware flash/repair ([9cfab51](https://github.com/SingTown/openmv-mcp/commit/9cfab518e1699015d51d04e746991cc1cf896918))


### Bug Fixes

* **server:** unblock streaming endpoints on shutdown ([af891fc](https://github.com/SingTown/openmv-mcp/commit/af891fc5b498161933dd949deb1d2ae1abff635b))

## [1.3.0](https://github.com/SingTown/openmv-mcp/compare/v1.2.0...v1.3.0) (2026-04-13)


### Features

* add frame_enable tool to toggle frame streaming ([2a43fdc](https://github.com/SingTown/openmv-mcp/commit/2a43fdc1171333520d1ba0238f0b46f8b6133dc0))
* guard camera map with mutex and add MCP request logging ([943fcee](https://github.com/SingTown/openmv-mcp/commit/943fceece41d99350cfb89c03e1bee36576a1d4d))
* implement findDrivePath for Windows ([abb6578](https://github.com/SingTown/openmv-mcp/commit/abb65787fc7594a4f98a5d79e9d5e4260749b8a4))
* include connection state in camera_list output ([f5426a7](https://github.com/SingTown/openmv-mcp/commit/f5426a71f562a47039f66e08093324c71f387e2b))
* include latest firmware version in camera_info output ([1096601](https://github.com/SingTown/openmv-mcp/commit/1096601278e4eb500fb63d6642896202346d4522))
* rename /ws/script to /ws/status with connection state ([3674b53](https://github.com/SingTown/openmv-mcp/commit/3674b53e24a2f6b87c29105dc540f4a8244a5a23))
* switch Windows serial port to overlapped I/O ([d334b1b](https://github.com/SingTown/openmv-mcp/commit/d334b1bf81f35421a377549fd5daff3a1c66755c))

## [1.2.0](https://github.com/SingTown/openmv-mcp/compare/v1.1.0...v1.2.0) (2026-04-13)


### Features

* add Windows daemon mode via CreateProcess detached respawn ([ee12bcb](https://github.com/SingTown/openmv-mcp/commit/ee12bcb8e700edf40e85ead80819259ab2ed83d2))
* integrate spdlog for structured logging ([71abefe](https://github.com/SingTown/openmv-mcp/commit/71abefe3d8d5fe10aac51d342cbdc06aa236da50))

## [1.1.0](https://github.com/SingTown/openmv-mcp/compare/v1.0.0...v1.1.0) (2026-04-12)


### Features

* exit 0 when port is already bound by an openmv-mcp server ([20f02a7](https://github.com/SingTown/openmv-mcp/commit/20f02a72d856b580c5d69fd0bd39473ca4ab24de))

## 1.0.0 (2026-04-12)


### Features

* add Camera callback system and WebSocket endpoints for real-time push ([d903a2d](https://github.com/SingTown/openmv-mcp/commit/d903a2dab052793928e2f4b519a0c5f82af9daf4))
* add camera path and USB drive path discovery to CameraInfo ([489a76e](https://github.com/SingTown/openmv-mcp/commit/489a76eb11dd9c5f46779814e8aa6b370e57afbc))
* add camera reset/boot support and camera_reset MCP tool ([17037b5](https://github.com/SingTown/openmv-mcp/commit/17037b5bfa3f80b451798ff4bf66e67db9935667))
* add cancellation support for MCP tool calls ([abb2486](https://github.com/SingTown/openmv-mcp/commit/abb2486c83bb5deef40ad401ad015e51962d2515))
* add cross-platform subprocess module with streaming output capture ([4226453](https://github.com/SingTown/openmv-mcp/commit/4226453ea3d1787d95bd2cc33c900823b01dc915))
* add daemon mode for POSIX background execution ([4c417d0](https://github.com/SingTown/openmv-mcp/commit/4c417d0d4aadcebae96e34d973dcbd4970c6b7d2))
* add firmware flash and repair MCP tools ([6aa8556](https://github.com/SingTown/openmv-mcp/commit/6aa855613c014a67b4bfa81c891537f008a082a2))
* add license check and registration for OpenMV cameras ([9b54643](https://github.com/SingTown/openmv-mcp/commit/9b54643afcb71dd25dfec8ac016c96b2f1364ac3))
* add list_cameras tool for USB serial port camera discovery ([8cb1a70](https://github.com/SingTown/openmv-mcp/commit/8cb1a7058d4813ebaf3570332e5cabbca730dd9e))
* add protocol layer, camera info, and utility modules ([981a0e5](https://github.com/SingTown/openmv-mcp/commit/981a0e50335cdf09725e59e579f9181fc458499b))
* add read_frame tool with multi-format JPEG conversion ([7b6c8ab](https://github.com/SingTown/openmv-mcp/commit/7b6c8abd78cfa95407deab1e0976566b73e8a5d1))
* add release automation and --version flag ([fb1651c](https://github.com/SingTown/openmv-mcp/commit/fb1651c46a485b7db8a259a3807377e4cb96f7d5))
* add resource ([1863fc5](https://github.com/SingTown/openmv-mcp/commit/1863fc5cf0576c7b23004a0eacfba866fea75697))
* add script execution, terminal I/O, and graceful shutdown ([f453127](https://github.com/SingTown/openmv-mcp/commit/f453127d0835715f929948844b39a5d469f1bc96))
* add script_save tool to write main.py to camera USB drive ([4b5553b](https://github.com/SingTown/openmv-mcp/commit/4b5553b542c696fbadeb8eb1deab4869d7a24455))
* add serial port communication and camera connect/disconnect tools ([279fa99](https://github.com/SingTown/openmv-mcp/commit/279fa99f6f75ef2164ed103745c5457c5dc3d491))
* add Streamable HTTP streaming support for MCP tool calls ([3c1813b](https://github.com/SingTown/openmv-mcp/commit/3c1813bf45bd3b81395c6c788633ebd0c7a7950a))
* add Utf8Buffer for safe UTF-8 terminal streaming ([6fb3cfd](https://github.com/SingTown/openmv-mcp/commit/6fb3cfd7f33ddf13ddd485b193393854582d7b41))
* implement MCP server with HTTP transport and JSON-RPC dispatch ([684329f](https://github.com/SingTown/openmv-mcp/commit/684329ff34c9e4a6e88c6825df81f546c857a7a2))
* implement Windows camera discovery via Setup API ([430cd5b](https://github.com/SingTown/openmv-mcp/commit/430cd5b7f8b385cc99fd683391e2ac80536e4a26))
* implement Windows serial port and fix frame transfer protocol ([#1](https://github.com/SingTown/openmv-mcp/issues/1)) ([ad1d046](https://github.com/SingTown/openmv-mcp/commit/ad1d0461fc11020a7dc636947f77e40cb8294252))
* implement Windows subprocess with job object and pipe-based I/O ([85a3839](https://github.com/SingTown/openmv-mcp/commit/85a38398df4ba28f35ed2693f77c5b67e6c81476))
* use GET_STATE/channelPoll for protocol polling and script status ([63f4c85](https://github.com/SingTown/openmv-mcp/commit/63f4c85cd2abf7fc01be59bfefe2082fcbd88e9c))
