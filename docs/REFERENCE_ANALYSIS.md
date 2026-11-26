# 参考样本分析

## 分析目的

本文件记录两个本地参考样本的可观察功能、机制线索和许可证边界。目标是为 win-butler 定义行为与测试场景，不复制参考软件实现。

## 本地样本

### W11ClassicMenu

- 路径：`E:\Software\W11ClassicMenu\W11ClassicMenu.exe`
- 文件版本：`1.2.0.0`
- 厂商字段：`www.sordum.org`
- 附带配置：`W11ClassicMenu.ini`
- 配置中可见的一期相关能力：
  - 启用 Windows 11 经典右键菜单样式
  - 启用 Windows 11 默认右键菜单样式
  - 应用到所有用户
  - 重启资源管理器以生效
- 配置中还有任务栏位置、自动隐藏等能力，不属于 win-butler 一期范围。

### ContextMenuManager

- 路径：`E:\Software\ContextMenuManager.NET.4.0.exe`
- 文件版本：`3.3.3.1`
- 产品名称：`Windows右键管理`
- SHA-256：`1352C2222D2D28DD1A8824334F16C0676DFE88CD7211DE873EC9B125F6782EC7`
- 公开仓库：`BluePointLilac/ContextMenuManager`
- 公开仓库许可证：GPL-3.0
- 公开 README 中的主要能力：
  - 文件、文件夹、新建、发送到、打开方式、自定义文件格式、IE、WinX 等菜单项
  - 启用、禁用、改名、改图标、定位注册表、定位文件、永久删除
  - 自定义添加菜单项和命令
- 公开源码目录体现出的模块：
  - `ShellItem` / `ShellList`：传统 shell 动词
  - `ShellExItem`：COM ContextMenuHandlers
  - `ShellNewItem` / `ShellNewList`：新建菜单
  - `SendToItem` / `SendToList`：发送到菜单
  - `OpenWithItem` / `OpenWithList`：打开方式
  - `WinXItem` / `WinXList`：WinX 菜单
  - `UwpModeItem`：UWP/包相关模式
  - `GuidBlockedItem` / `GuidBlockedList`：按 CLSID 管理阻止列表
  - `ExplorerRestarter`：资源管理器重启

## 机制结论

### 经典与现代样式

Windows 11 的现代菜单与经典菜单可以通过当前用户的 CLSID 覆盖项切换：

`HKCU\Software\Classes\CLSID\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}\InprocServer32`

当前机器上 `HKLM` 中的同名项指向：

`C:\Windows\System32\Windows.UI.FileExplorer.dll`

win-butler 只在 `HKCU` 创建或删除覆盖项来实现当前用户切换。机器级切换需要单独设计、显式确认和回滚验证。

### 传统菜单项

一期至少扫描以下位置：

- `*\shell`
- `AllFilesystemObjects\shell`
- `Directory\shell`
- `Directory\Background\shell`
- `Drive\shell`
- `Folder\shell`
- `DesktopBackground\shell`
- `SystemFileAssociations\<ext>\shell`
- `<ProgID>\shell`
- 对应位置的 `shellex\ContextMenuHandlers`

`HKCU\Software\Classes` 与 `HKLM\Software\Classes` 分别扫描。`HKCR` 只用于解析合并视图，不用于写入。

### Packaged COM

本机可观察到大量注册：

- `HKLM\Software\Classes\PackagedCom\Package\<PackageFullName>\Class\<CLSID>`
- `HKLM\Software\Classes\PackagedCom\Package\<PackageFullName>\Server\<Index>`

`PackagedCom` 中包含许多非右键菜单用途的 COM 类。不能把每个 `Class` 都当作右键菜单项。正确做法是读取已安装包的 `AppxManifest.xml`，筛选 `windows.fileExplorerContextMenus` 扩展，再与注册表中的 CLSID 和 Server 信息连接。

### 禁用策略

候选机制按安全性排序：

1. 传统 `shell` 动词：验证并优先使用 `LegacyDisable`
2. 传统 `shellex`：使用 Shell Extensions Blocked 列表
3. Packaged COM：先验证 HKCU Blocked 列表是否对现代菜单生效
4. 永久删除：仅用于用户自建项，且必须先备份

Packaged COM 的可用禁用机制必须在 Windows 11 多版本虚拟机上验证，不能仅凭注册表存在就假定有效。

## 许可证边界

ContextMenuManager 的公开源码使用 GPL-3.0。win-butler 采取以下边界：

- win-butler 已确定使用 MIT 许可证，因此不能直接复制 GPL-3.0 代码
- 可以阅读公开 README、文档和源码以理解 Windows 机制
- 不复制其源码、资源、文案、图标或数据结构
- 不以反编译结果直接生成 win-butler 代码
- 只记录可独立验证的注册表路径、COM 接口、行为矩阵和测试场景
- 如果未来需要直接复用 GPL 代码，必须先单独评估并调整 win-butler 的许可证

Sordum 样本是本地二进制参考。仅做黑盒行为和公开注册表机制对照，不重新分发样本，不复制其资源。

## IDA 使用边界

当前环境未发现可直接调用的 IDA 安装。规划阶段不需要 IDA。

后续如果静态分析确有必要：

- 只在隔离的 `research/` 目录保存数据库和导出文件
- `research/` 不提交到仓库
- 只提取协议、注册表路径、功能状态机等互操作事实
- 不把反编译代码片段移植进产品源码
- 分析记录需注明样本版本、哈希和结论来源

## 对 win-butler 的差异化目标

参考软件各自侧重不同：

- Sordum 聚焦样式切换，功能窄但路径直接
- ContextMenuManager 聚焦传统菜单项，覆盖面广，但现代 Packaged COM 不是其核心

win-butler 一期的差异化目标是：

- 在同一份目录模型中统一传统 `shell`、`shellex` 与 Packaged COM
- 明确显示每项的注册来源、包身份、文件路径、发布者与风险
- 所有写操作都有预检、快照、验证、回滚和撤销
- UI 默认不提权，危险操作通过受控代理完成
