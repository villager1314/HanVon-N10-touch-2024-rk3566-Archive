# 每应用 E-Ink 刷新策略

## 数据来源

实机只读提取的数据库位于：

```text
/data/user_de/0/com.android.systemui/databases/Eink
```

原始数据库不进入公开仓库。分析副本显示数据库包含 `EinkSettings` 表：

```sql
CREATE TABLE EinkSettings (
    id integer primary key autoincrement,
    package_name text,
    app_dpi integer default '-1',
    is_dpi_setting integer default '0',
    is_refresh_setting integer default '0',
    refresh_mode integer default '-1',
    refresh_frequency integer default '0',
    is_contrast_setting integer default '0',
    app_contrast real default '0',
    app_anim_filter integer default '0',
    app_bleach_mode integer default '0',
    app_bleach_text_plus integer default '0',
    app_bleach_icon_color integer default '0',
    app_bleach_cover_color integer default '0',
    app_bleach_bg_color integer default '0',
    app_blacken integer default '0',
    app_night_mode integer default '0'
);
```

## 离线反编译结果

SystemUI 中的 `EinkRefreshDialog` 和相关管理类会：

1. 根据当前前台包名查询 `EinkSettings`；
2. 更新 `refresh_mode` 和 `refresh_frequency`；
3. 通过 `EinkSettingsManager.setEinkMode()` 应用模式；
4. 调用 `refreshAll()` 执行全刷；
5. 在部分路径更新 `persist.vendor.fullmode_cnt`。

已看到的刷新模式数值包括 `0`、`7`、`12`、`13`、`14`、`15`，但在完成实机逐项映射前，不应仅凭数值猜测模式名称。

## 当前推断

系统应用通常已经存在数据库行，但保持：

```text
is_refresh_setting = 0
refresh_mode = -1
```

部分可配置第三方应用则是：

```text
is_refresh_setting = 1
refresh_mode = 13
```

这表明限制很可能位于 SystemUI 的应用增强界面或策略层，而不是 E-Ink 驱动完全拒绝系统应用。

## 安全边界

尚未对原数据库做写入实验。不要直接覆盖数据库，也不要随意执行 `chmod`、`chown` 或修改 SELinux 标签。SystemUI 数据文件的权限或上下文错误可能导致界面崩溃、刷新控制失效或卡开机。

后续应按以下顺序验证：

1. 完整记录数据库及父目录的权限、属主和 SELinux 上下文；
2. 通过 SystemUI 自身 ContentProvider/API 测试，而不是替换数据库文件；
3. 先选择非关键系统应用，避免 Launcher、Settings、SystemUI；
4. 准备可由 Loader 单刷 boot 的恢复路径；
5. 记录修改前后数据库行、系统属性和显示效果。

