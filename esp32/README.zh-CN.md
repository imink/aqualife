# AquaLife ESP32 / M5StickS3 固件

这个目录包含 AquaLife 水族馆的硬件固件版本。

英文文档见 [README.md](README.md)。

## 需求

- M5StickS3 / ESP32-S3 设备
- PlatformIO
- USB 数据线

## VS Code 工作区

开发完整项目时，请在 VS Code 中打开 [../aqualife.code-workspace](../aqualife.code-workspace)。

这个 workspace 会把仓库根目录和 [esp32](.) 作为两个独立 folder 加入。这样既能保留 Web app，又能让 PlatformIO 把 [platformio.ini](platformio.ini) 识别为顶层 workspace 项目。

如果只打开仓库根目录，PlatformIO 可能不会自动识别嵌套的 ESP32 项目。

## 安装固件

以下步骤面向想把 AquaLife 安装到 M5StickS3 上的玩家。

1. 安装 [Visual Studio Code](https://code.visualstudio.com/) 和 PlatformIO 扩展。
2. 用 USB 数据线把 M5StickS3 连接到电脑。
3. 在 VS Code 中打开这个仓库。
4. 打开 PlatformIO 面板。
5. 选择 `m5sticks3 > General > Upload` 来刷入固件。
6. 如果刷入后还想查看串口日志，选择 `m5sticks3 > General > Upload and Monitor`。

如果刷入后端口发生变化，请拔下并重新连接设备，然后再次运行 `Upload and Monitor`。

## 下载固件 Release

发布版 M5StickS3 固件会作为 GitHub Release assets 发布。

1. 在 GitHub 上打开这个仓库。
2. 打开 `Releases` 页面。
3. 打开最新 release，例如 `v1.0.0`。
4. 下载 `aqualife-m5sticks3-<version>-...` assets。

每个 release 包含：

- `aqualife-m5sticks3-<version>-firmware.bin`
- `aqualife-m5sticks3-<version>-firmware.elf`
- `aqualife-m5sticks3-<version>-bootloader.bin`
- `aqualife-m5sticks3-<version>-partitions.bin`
- `aqualife-m5sticks3-<version>-build_info.h`
- `SHA256SUMS.txt`

可以使用 PlatformIO 或其他 ESP32 刷写工具安装下载到的二进制文件。

## 游玩

- 按钮 A：喂鱼。
- 按钮 B：切换到下一个 app 页面。
- 短暂摇晃设备：吓到鱼；鱼会隐藏几秒钟，然后回来。
- 几秒钟没有操作后，屏幕会变暗以节省电量。

水族馆会把鱼的核心状态保存到设备存储中，因此饥饿度和快乐度会在重启后保留。被吓到、隐藏这类临时状态不会在重启后恢复。

## 固件信息

固件会在启动时打印版本号、git commit 和 UTC 构建时间，例如：

```txt
AquaLife ESP32 firmware v1.0.0 (abc1234)
Build time: 2026-05-27T00:00:00Z
```

## 构建资源

在项目根目录运行：

```bash
npm run esp32:assets
```

这会把 `../public/assets/` 中的 PNG sprite sheet 转换成 `esp32/include/sprites.h`，格式为 RGB565 像素数据和 alpha mask。

在 VS Code 的 PlatformIO 面板中，也可以使用自定义任务：

```txt
m5sticks3 > Custom > Build Assets
```

`Build` 和 `Upload` 在编译或上传前也会自动运行资源转换。

GitHub Releases 包含正式发布的固件二进制文件。创建 `v*` 标签（例如 `v1.0.0`）会运行 release workflow，并把固件 assets 上传到对应 release。

## 构建固件

```bash
npm run esp32:build
```

## 上传固件

```bash
npm run esp32:upload
```

## 监看串口日志

```bash
npm run esp32:monitor
```

## Sprite 格式

使用项目统一的 sprite 规则：

- 横向 sprite sheet
- 4 帧
- 每帧 `32x32`
- 总尺寸 `128x32`
- 透明 PNG
- 鱼朝右
- 尾巴/鱼鳍做动画，身体尽量稳定

当前转换器包含：

- `public/assets/clownfish_sprite_sheet.png`
- `public/assets/whale_sheet.png`
- `public/assets/hammerhead_shark_sprite.png`
- `public/assets/amazon_sword_sprite_sheet.png`
- `public/assets/cabomba_sprite_sheet.png`

## 运行行为

- 显示：`240x135` 横屏
- FPS：固定 `25`
- 渲染使用 16 位离屏 framebuffer（`M5Canvas`），并把完整帧推送到 LCD，以避免闪烁
- 动画：4 帧 sprite sheet
- 按钮 A：喂鱼
- 按钮 B：切换到下一个 app 页面
- 摇晃：鱼会快速游走、隐藏，然后陆续回来
- 状态持久化：关键事件发生时，鱼的饥饿度和快乐度会保存到 NVS
- 渲染：RGB565
