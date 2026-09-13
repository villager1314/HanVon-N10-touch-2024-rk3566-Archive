# Hanvon N10 Touch 2024 / RK3566 研究记录

> [!CAUTION]
> **请勿直接刷入本仓库中的实验性 TWRP，也不要把其他 RK3566 设备的 Recovery 刷到本机。**
>
> 目前的 TWRP 仍处于真机适配阶段。已知测试版本曾出现 ADB 不可用、触摸坐标错位、重启后再次进入 Recovery，以及停留在“N10 Touch”开机画面等问题。特别是使用墨水屏刷新模式 13 的构建已经确认无法正常启动，请勿刷入或传播为“可用版本”。
>
> GitHub Actions 显示构建成功，只代表镜像能够编译和打包，**不代表已经通过真机验证**。除非 Release 明确标注“已验证”，否则所有生成的 Recovery 镜像都应视为开发测试产物。
>
> 测试前必须确认设备能够进入 Rockchip Loader 模式，并准备与本机固件版本匹配的原厂 Recovery 备份。不要写入 `uboot`、`trust`、`waveform`、`dtbo`、`vbmeta`、`super` 或 `userdata`。出现异常时应优先通过 Loader 仅恢复 `recovery` 分区，切勿使用“擦除所有”。

本仓库记录汉王 N10 Touch 2024 电纸书的设备识别、Android 启动链、Recovery/Fastboot/Loader 模式、分区备份、Magisk Root、蓝牙故障和墨水屏应用刷新策略研究。

内容以实机观察、ADB 输出、升级包和离线分析为依据。仓库不会公开设备唯一数据、用户数据、原厂 APK 或完整固件。为便于同型号、同固件设备恢复，GitHub Release 可单独提供最小必要镜像包；这些第三方二进制不适用本仓库的 MIT License。

> [!WARNING]
> RK3566 只是 SoC 型号，不代表不同设备的 Loader、U-Boot、Trust、DTB、屏幕波形或分区布局可以混用。写错 `uboot`、`trust`、`waveform`、`dtbo`、`vbmeta` 或 `super` 可能造成黑屏、无法显示、数据丢失甚至需要拆机进入 Maskrom。任何写入前都应备份原分区并校验 SHA-256。

## 目标设备

| 项目 | 实测值 |
| --- | --- |
| 品牌/型号 | 汉王 N10 Touch 2024 |
| Product / Device | `rk3566_eink` / `rk3566_eink` |
| SoC | Rockchip RK3566，ARM64 |
| Android | Android 11，`RQ2A.210505.003` |
| 构建类型 | `user/release-keys` |
| 原厂 Recovery | Android Recovery，独立 `recovery` 分区 |
| 底层下载模式 | Rockchip Loader，可由 Rockusb 驱动识别 |
| Root | Magisk 30.7 修补 `boot.img` 已验证成功 |
| 无线模块 | AP6256 / Broadcom BCM43456 组合模块 |

公开记录中不保留设备序列号、账号、MAC 地址或其他唯一标识。

## 研究历程

### 1. ADB 与系统组件识别

Windows 设备管理器中的 `ADB Interface` 只是当前驱动提供的接口名称；是否显示为 `Android ADB Interface` 不决定 ADB 能否使用。应以 `adb devices -l` 是否识别设备并完成设备端授权为准。

研究中识别过若干容易混淆的组件：

- `com.android.launcher3`：AOSP Launcher 包名；
- `com.android.settings/.FallbackHome`：系统在真正桌面尚未可用时使用的临时 Home；
- `ginlemon.flowerfree`：Smart Launcher；
- `hanvon.aebr.camera`：汉王相机/扫描相关应用组件；
- 实机默认前台桌面为 `hanvon.aebr.hvLauncher`。

### 2. 升级包与分区布局

原厂 OTA 包包含 `boot.img`、`dtbo.img`、`uboot.img`、动态分区操作列表以及 `system/vendor/product/odm` 的 `new.dat.br` 数据。

Loader 模式读取到的 GPT 关键分区如下。LBA 单位为 512 字节扇区：

| 分区 | 起始 LBA | 扇区数 | 容量 |
| --- | ---: | ---: | ---: |
| `uboot` | `0x4000` | `0x2000` | 4 MiB |
| `trust` | `0x6000` | `0x2000` | 4 MiB |
| `waveform` | `0x8000` | `0x1000` | 2 MiB |
| `misc` | `0x9000` | `0x2000` | 4 MiB |
| `dtbo` | `0xB000` | `0x2000` | 4 MiB |
| `vbmeta` | `0xD000` | `0x800` | 1 MiB |
| `boot` | `0xD800` | `0x10000` | 32 MiB |
| `security` | `0x1D800` | `0x2000` | 4 MiB |
| `recovery` | `0x1F800` | `0x30000` | 96 MiB |
| `super` | `0x1D7800` | `0x614000` | 3.04 GiB |

这台设备的 GPT 分区名是单独的 `boot`，未观察到 `boot_a` / `boot_b`。文件名本身不会决定刷入目标，刷写工具使用的是分区名或起始地址。

### 3. Recovery、Fastboot 与 Loader

设备可以进入原厂 Android Recovery。首先出现倒地机器人和“没有指令（No command）”，随后可进入文字菜单。该 Recovery 显示：

```text
HANVON/rk3566_eink/rk3566_eink
11/RQ2A.210505.003/...
user/release-keys
Secure boot - yes
```

菜单包含 `Reboot system now`、`Enter fastboot`、`Reboot to bootloader` 等项目，但实机上通过按键或菜单进入 Fastboot 并不稳定，不能把普通安卓设备的“音量上/下”映射直接套到翻页键。

Rockchip Loader 是这次可靠的底层恢复路径。设备已处于 Loader 时，不需要也不应随意加载其他板级的通用 Loader。RKDevTool v2.96 在手工编辑下载行时曾闪退，后改用新版工具完成单分区写入。

实机照片和入口说明见 [`docs/boot-modes.md`](docs/boot-modes.md)。已确认的三条路径是：

- 返回键 + 电源键：出现“没有指令（No command）”；
- `adb reboot fastboot`：进入 Android Fastboot（fastbootd）菜单；
- fastbootd 中选择 `Reboot to bootloader`：实际回到 Android Recovery 菜单，而不是可稳定使用的传统 U-Boot fastboot。

### 4. Magisk Root

从匹配当前固件版本的原始 `boot.img` 制作 Magisk 30.7 修补镜像。写入后成功启动，`su -c id` 返回 Magisk root 上下文，证明：

- `boot` 分区地址和镜像格式正确；
- 设备允许启动该 Magisk 修补镜像；
- 没有必要为了 Root 修改 `uboot`、`trust`、`recovery` 或 `super`；
- Root 成功不等于所有后续 DTB 修改都安全。

不要传播修补后的 `boot.img`：它与具体固件版本绑定，并可能包含不应重新分发的原厂内核内容。仓库仅保留哈希和方法记录。

### 5. 蓝牙故障调查

现象是点击蓝牙开关后没有正常开启。离线检查升级包与运行系统后确认：

- 实际组合模块为 AP6256 / Broadcom BCM43456；
- 当前 `/vendor/lib64/libbt-vendor.so` 属于 Broadcom/AMPAK 路线，并含 BCM4345C5 相关字符串；
- 升级包中的 Realtek `rtl8761bt_fw`、`rtl8761bt_config` 和 `rtkbt.conf` 更像备用文件，不能据此认定设备使用 Realtek；
- 蓝牙 UART 为 `/dev/ttyS1`；日志曾出现 `ttyS1 set divisor fail`，且 RX 计数没有正常增长；
- Android 能启动不能排除无线子模块、UART、复位/供电或固件下载链故障。

曾尝试移除 UART1 的 DMA 属性：

1. 第一版只修改了 boot 中常规 `dtb`，但实机设备树没有变化；
2. 进一步确认该 Rockchip boot 镜像还在 `second` 资源中携带 DTB；
3. 第二版修改 `second` 内嵌 DTB 后导致设备循环重启；
4. 因风险大于收益，停止通过 boot/DTB 继续调试蓝牙。

结论：下一阶段应优先使用只读日志、运行时参数和可撤销的 Magisk 模块，不再直接修改 boot 内设备树。

### 6. Loader 救砖验证

写入第二版实验 boot 后设备无法进入 Android，但 Recovery、U-Boot 和 Loader 仍然可用，说明故障局限于 boot 启动路径。

最终恢复步骤：

1. 进入 Rockchip Loader；
2. 使用可稳定识别 Loader 的 RKDevTool；
3. Loader 行保持未勾选；
4. 只勾选 `boot`；
5. 使用起始地址 `0x0000D800`；
6. 写入此前已验证能启动的 Magisk boot；
7. 写入成功后重启，设备恢复开机。

这次恢复证明 Loader 可作为 boot 实验失败后的救援路径，但不代表其他关键分区可以无风险试刷。

### 7. 墨水屏刷新策略

系统注册了 Binder 服务：

```text
eink: [android.os.IEinkManager]
```

SystemUI 的设备保护存储中存在刷新策略数据库：

```text
/data/user_de/0/com.android.systemui/databases/Eink
```

数据库表 `EinkSettings` 以 `package_name` 保存每应用参数，已确认字段包括：

```text
is_refresh_setting
refresh_mode
refresh_frequency
is_contrast_setting
app_contrast
app_anim_filter
app_bleach_mode
app_blacken
app_night_mode
```

第三方应用启用刷新设置后常见记录为：

```text
is_refresh_setting = 1
refresh_mode = 13
```

而系统应用通常已有数据库行，但 `is_refresh_setting = 0`、`refresh_mode = -1`。离线反编译 SystemUI 还确认 `EinkRefreshDialog` 会更新这些字段，并通过 `EinkSettingsManager` 设置 E-Ink 模式和执行全刷。

因此，“系统应用不能修改刷新率”更可能是 SystemUI 界面或策略限制，并非面板本身不支持。当前仅完成只读定位，尚未向原数据库写入测试值。直接替换数据库、修改权限、属主或 SELinux 标签可能导致 SystemUI 异常甚至卡开机，后续测试必须先记录元数据并设计可恢复方案。

## 已确认结论

- 确认设备为汉王 N10 Touch 2024 / RK3566 / Android 11；
- 确认设备使用独立 `boot` 和 `recovery` 分区，不是 `boot_a` / `boot_b`；
- 已读取 GPT 并备份关键分区；
- Magisk 30.7 修补 boot 已成功取得 Root；
- 原厂 Recovery 与 Rockchip Loader 可用；
- Loader 按 `0xD800` 单刷 boot 已成功恢复一次启动失败；
- 蓝牙使用 AP6256 / BCM43456 路线，故障集中在 UART/固件初始化链；
- 修改 boot 内 `second` DTB 的 UART1 DMA 实验会导致循环重启，不应复用；
- 每应用刷新配置位于 SystemUI 的 `EinkSettings` 数据库，系统应用限制仍待安全验证。

## 仓库内容

- `README.md`：研究过程、实测结论和风险说明；
- `checksums/README.md`：本地镜像及备份的 SHA-256，仅用于版本识别；
- `docs/eink-refresh.md`：每应用刷新策略的数据库结构与离线分析；
- `tools/fdt_remove_props.py`：研究过程中编写的 FDT 属性移除脚本，仅供离线研究；
- `NOTICE.md`：第三方材料和固件许可说明。

## Release 镜像包

Release 中的最小恢复/Root 包仅面向与本仓库记录完全匹配的 N10 Touch 2024 固件，包含原厂 boot、已验证的 Magisk 30.7 boot 和原厂 recovery。刷写前必须核对 [`checksums/README.md`](checksums/README.md)；不要用于其他 RK3566 设备。

## 不包含的内容

- 原厂完整固件、OTA 包、APK 和动态分区镜像；
- 已知会循环重启的实验 boot 镜像；
- 从实机提取的分区镜像、数据库和日志原件；
- `userdata`、账号、序列号、MAC、密钥及其他设备唯一信息；
- 反编译生成的第三方源代码。

## 许可

本仓库作者原创的文档和脚本采用 [MIT License](LICENSE)。设备固件、Android、Magisk、Rockchip 工具、JADX、驱动、APK、字体、商标以及其他第三方材料仍受各自许可证和权利声明约束，不因本仓库记录研究结果或哈希而获得重新授权。详见 [NOTICE.md](NOTICE.md)。

