# 实验性 TWRP 构建

本目录保存汉王 N10 Touch 2024 实验性 TWRP 的可复现构建文件。构建过程复用原厂 Recovery 的内核、DTB、第二阶段数据和 Recovery DTBO，仅重新构建 TWRP ramdisk、用户空间以及 Rockchip 墨水屏显示后端。

早期版本已在真机上成功进入 TWRP 界面，但当时仍存在 ADB 不可用、触摸坐标错位以及无法正常重启至 Android 等问题。每次刷写测试前，请确保设备仍可进入 Rockchip Loader 模式，并在电脑上保留原厂 Recovery 镜像。

## USB 与 ADB

设备专用 init 文件只负责提供 Rockchip DWC3 控制器地址，以及从原厂 Recovery 复用的 USB Gadget 参数。adbd、FunctionFS 和 configfs USB 状态机由 TWRP 公共 init.rc 管理。

不要在设备 init 文件中重复执行 none → adb 切换，否则可能与公共 on fs 触发器发生时序竞争，导致 ffs.adb 未能正确连接。

## BCB 与重启保护

构建补丁把 TWRP 原有的一次性 BCB 清除操作移到了图形界面初始化之前。它不会改变 misc 分区偏移，也没有添加第二套 BCB 实现。

这样即使墨水屏第一次更新时发生阻塞，设备也不会因为 misc 中仍保留 boot-recovery 请求而持续进入 Recovery，仍可在下一次重启时尝试启动 Android。

目前继续使用已成功显示 TWRP 界面的墨水屏模式 7。模式 13 曾导致设备停留在开机画面，因此已撤回。

## 分区结构

对 1.00.84 原厂固件中 libbootloader_message.so 的离线分析确认：

- 2 KiB BCB 位于 misc 分区字节偏移 0；
- wipe-package 区域位于偏移 0x4000；
- 通用 Virtual A/B 消息区域位于偏移 0x8000。

设备实际采用 **A-only 动态分区**。原厂库包含通用 Virtual A/B 接口，并不表示该设备采用 Virtual A/B 分区架构。构建过程保持上游 TWRP/AOSP 的上述偏移，不对其进行修改，以符合原厂 Recovery 行为。

## 墨水屏接口

墨水屏后端依据 1.00.84 固件中观察到的 ABI 实现：

- 设备节点：/dev/ebc
- 映射大小：0x01400000
- ioctl：0x7000、0x7001、0x7002、0x7010
- ebc_buf_info 结构大小：68 字节
- 输出格式：Y4/Y8
- 当前刷新模式：模式 7（局部 GC16）

## 风险说明

GitHub Actions 构建成功只表示源码能够编译和打包，并不能证明显示、触摸、ADB、分区挂载、数据解密及重启功能均已在真机上验证。

device/.../prebuilt/ 中的原厂二进制组件仅用于设备兼容与 Recovery 研究，具体说明请参阅仓库中的 NOTICE 文件。
