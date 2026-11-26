# 上下文菜单管理技术设计

> 本文件记录上下文菜单模块的技术设计。全产品范围、开发顺序、验收标准和逐期 AI 提示词见[开发实施方案](DEVELOPMENT_PLAN.md)；开发规则见根目录 [AGENTS.md](../AGENTS.md)。项目验收以本机 Windows 构建、启动和核心流程可用为准，不要求端到端测试或虚拟机矩阵。

## 1. 文档状态

- 版本：0.1
- 日期：2026-11-26
- 阶段：规划与技术验证
- 一期主题：Windows 右键菜单管理

## 2. 产品定义

win-butler 是 Windows 系统管理工具。一期只解决资源管理器右键菜单的两个核心问题：

1. Windows 11 右键菜单样式管理
2. 右键菜单项识别、解释与安全管理

一期不扩展到启动项、服务、计划任务、驱动、卸载残留清理或通用注册表编辑。

## 3. 一期目标

### 3.1 必须完成

- 检测当前 Windows 11 右键菜单样式
- 在当前用户范围切换现代样式与经典样式
- 检测资源管理器重启需求并提供安全重启入口
- 扫描文件、文件夹、文件夹背景、驱动器场景的菜单项
- 识别传统 `shell` 动词
- 识别传统 `shellex\ContextMenuHandlers`
- 识别 Windows 11 Packaged COM 右键扩展
- 展示菜单名称、类型、来源、范围、状态、发布者和注册位置
- 支持传统菜单项的启用与禁用
- 支持 Packaged COM 菜单项的启用与禁用，前提是技术验证通过
- 所有写操作支持快照、回滚和撤销
- 提供搜索、筛选、详情、错误状态和空状态
- 在开发机完成构建、启动和核心本机操作检查

### 3.2 应该完成

- 自定义文件扩展名和 ProgID 场景
- 新建、发送到、打开方式、WinX 场景的只读盘点
- 用户自建菜单项添加、编辑和删除
- 批量启用与禁用
- 变更历史与本地审计日志
- 中英文界面与文档
- 便携版 ZIP 打包

### 3.3 暂不完成

- 图标覆盖处理程序
- 属性页处理程序
- 缩略图与预览处理程序
- 仅在目标 Windows 版本仍提供该场景时支持 IE 右键菜单；否则标记为不适用
- 注册表通用编辑器
- 第三方 Shell 扩展的自动修复或自动卸载
- 云端同步、遥测和账号系统
- Windows 10 正式兼容性承诺

## 4. 平台与构建基线

- 操作系统：Windows 11 x86 32 位进程，运行于 x64 Windows 11 22H2 及以上
- 编译语言：C++20
- 编译器：MSVC 2022 或更新版本，最低工具集 19.34
- 构建系统：CMake 3.24+
- UI：EUI-NEO `v0.6.5`
- 架构：仅 x86，32 位进程
- 运行库：静态 MSVC 运行库，便携 ZIP 自包含
- 开发构建：Debug、RelWithDebInfo、Release
- 本机验收构建：Release + PDB

Windows 11 是唯一发布门禁。Windows 10 可以作为兼容性观察项，但不能阻塞一期发布。

## 5. 技术原则

### 5.1 Shell 层保持原生

Shell 识别层不依赖 .NET、Python 或 PowerShell 运行时。核心能力直接调用：

- Win32 Registry API
- COM API
- Shell API
- Windows 包与清单读取 API
- WinRT ABI，仅在包枚举没有稳定 Win32 替代时使用

允许通过 `RoGetActivationFactory` 调用 Windows Runtime ABI，但不引入 C#、WinUI 或托管运行时。

### 5.2 UI 与系统写入分离

UI 进程默认以普通用户权限运行。系统写入分为两类：

- 当前用户注册表：由 Shell 层在 UI 进程内执行
- 机器级注册表、机器级包或需要管理员的变更：交给独立提权代理

提权代理只接受结构化、白名单操作，不接受任意命令行或任意注册表路径。

### 5.3 只读优先

启动扫描不激活第三方 COM 服务器，不运行包内代码，不执行菜单命令。需要 COM 激活才能取得的显示名称、图标或启用状态，必须在后续独立探测阶段完成，并默认关闭。

### 5.4 禁用优先于删除

一期所有第三方菜单项默认只允许禁用。永久删除只适用于 win-butler 自己创建的项目，或用户明确打开高级模式并完成备份的项目。

### 5.5 所有写入可回滚

任何写入操作必须先形成计划，再快照，再提交，再验证。任一步失败都进入回滚路径。

## 6. 总体架构

以下目录图以当前仓库为准。功能实现优先扩展现有职责，不创建端到端测试、VM 测试或新的 fixture 目录。

```text
src/
  app/
    EUI-NEO desktop entry point, views, and view state
  shell/
    Windows Shell catalog, registry access, package manifests,
    and context-menu discovery/operations
  tools/
    command-line scanner and local diagnostics
tests/
  unit/
    existing focused unit checks only
fixtures/
  classic-shell/
  shellex/
  packaged-com/
docs/
```

If a later phase proves that shared transaction logic or an elevated operation host needs a distinct module, record that boundary and add a narrowly scoped subdirectory under `src/`; do not copy the earlier speculative directory tree into the repository wholesale.

### 6.1 CMake targets

Keep the current targets `win_butler`, `win_butler_shell`, and `win_butler_scan`. The existing `win_butler_tests` target may be used for narrow local checks but is not an acceptance gate. Keep fixture targets optional and limited to the existing `fixtures/` content. Add a new target only when a phase requires a separate executable or reusable library.

### 6.2 依赖方向

```text
app -> win_butler_shell
scan tool -> win_butler_shell
existing unit checks -> win_butler_shell
```

Shell/catalog code does not depend on EUI-NEO state. The UI should call typed operations rather than manipulating registry values directly. Introduce a narrow allowlisted elevation boundary only if an implemented operation actually requires it.

## 7. 数据模型

### 7.1 场景

```cpp
enum class MenuScene {
    File,
    AllFilesystemObjects,
    Directory,
    DirectoryBackground,
    Drive,
    Folder,
    DesktopBackground,
    Extension,
    ProgId,
    WinX,
    SendTo,
    New,
    OpenWith
};
```

### 7.2 菜单项种类

```cpp
enum class MenuItemKind {
    ShellVerb,
    ContextMenuHandler,
    PackagedComCommand,
    ShellNew,
    SendTo,
    OpenWith,
    WinX
};
```

### 7.3 菜单项记录

```cpp
struct MenuItemRecord {
    std::string stableId;
    std::string displayName;
    MenuScene scene;
    MenuItemKind kind;
    Hive hive;
    std::wstring registryPath;
    std::wstring clsid;
    std::wstring packageFullName;
    std::wstring packageFamilyName;
    std::wstring executablePath;
    std::wstring publisher;
    std::wstring version;
    EffectiveState state;
    CapabilitySet capabilities;
    RiskLevel risk;
    std::vector<Diagnostic> diagnostics;
};
```

### 7.4 稳定标识

稳定标识不使用数组下标或扫描顺序。建议组合：

- Shell 命令项：`hive + scene + registryPath`
- ShellEx：`hive + scene + clsid`
- Packaged COM：`packageFamilyName + clsid + verbId`
- SendTo：文件路径哈希或规范化路径
- New：ProgID 与注册路径

### 7.5 能力矩阵

每项必须明确声明可执行能力：

- `CanInspect`
- `CanEnable`
- `CanDisable`
- `CanEdit`
- `CanDelete`
- `CanReorder`
- `RequiresElevation`
- `RequiresExplorerRestart`

UI 不通过菜单项类型自行猜测能力，只读取能力矩阵。

## 8. Shell 识别层

### 8.1 注册表扫描范围

分别扫描 `HKCU\Software\Classes` 与 `HKLM\Software\Classes`。

| 场景 | 路径 |
| --- | --- |
| 所有文件 | `*\shell`, `*\shellex\ContextMenuHandlers` |
| 所有文件系统对象 | `AllFilesystemObjects\shell`, `AllFilesystemObjects\shellex\ContextMenuHandlers` |
| 文件夹 | `Directory\shell`, `Directory\shellex\ContextMenuHandlers` |
| 文件夹背景 | `Directory\Background\shell`, `Directory\Background\shellex\ContextMenuHandlers` |
| 驱动器 | `Drive\shell`, `Drive\shellex\ContextMenuHandlers` |
| 文件夹对象 | `Folder\shell`, `Folder\shellex\ContextMenuHandlers` |
| 桌面背景 | `DesktopBackground\shell`, `DesktopBackground\shellex\ContextMenuHandlers` |
| 文件类型 | `SystemFileAssociations\<ext>\shell` |
| ProgID | `<ProgID>\shell` |

`HKCR` 可以用于解析最终合并视图，但不能用于确定写入目标。UI 必须显示实际来源 Hive。

### 8.2 Shell 命令项解析

产品界面中的类型名称使用“Shell 命令项（Shell verb）”。`ShellVerb` 只作为代码中的类型标识；不得把“传统动词”作为面向用户的标签。

读取并解释：

- 默认值或 `MUIVerb`
- `Icon`
- `Command` 默认值
- `DelegateExecute`
- `Extended`
- `LegacyDisable`
- `ProgrammaticAccessOnly`
- `AppliesTo`
- `Position`
- `SubCommands`
- `MultiSelectModel`

`Command` 只作为文本展示和编辑对象，不执行。

### 8.3 ShellEx 解析

读取：

- ContextMenuHandlers 子键名称
- CLSID 默认值
- `CLSID\{...}\InprocServer32`
- `LocalServer32`
- `AppID`
- `TreatAs`
- `ProgID`
- Shell Extensions Approved
- Shell Extensions Blocked

解析 DLL 或 EXE 时只读取文件元数据、签名状态和版本信息，不加载该模块。

### 8.4 Packaged COM 解析

扫描：

- `HKLM\Software\Classes\PackagedCom\Package`
- `HKCU\Software\Classes\PackagedCom\Package`

每个包读取：

- `PackageFullName`
- `Class\<CLSID>`
- `Class\<CLSID>\DllPath`
- `Class\<CLSID>\Verbs`
- `Server\<Index>`
- `ApplicationId`
- `DisplayName`
- `ApplicationDisplayName`

随后通过 Windows 包 API 获取安装位置并读取 `AppxManifest.xml`。只保留声明了 `windows.fileExplorerContextMenus` 的扩展。

### 8.5 包清单解析安全

- 禁用 DTD 和外部实体
- 限制 XML 大小和嵌套深度
- 不执行清单内任何命令
- 对清单字段做长度和字符校验
- 包更新后重新扫描，稳定标识使用 Package Family Name 而不是完整版本名

### 8.6 扫描性能

目标：

- 常规开发机冷扫描小于 2 秒
- 热刷新小于 1 秒
- UI 不阻塞
- 扫描取消能在 100 毫秒内响应
- 单包解析失败只产生诊断，不终止全量扫描

扫描在后台线程执行，结果通过不可变快照返回 UI。UI 线程只消费完成后的快照。

## 9. 写入与事务模型

### 9.1 操作类型

```cpp
enum class RegistryActionType {
    CreateKey,
    DeleteKey,
    SetValue,
    DeleteValue
};
```

每条操作必须包含：

- 目标 Hive
- 规范化路径
- 视图标志
- 值名、值类型、值数据
- 预期旧值
- 是否为创建操作

### 9.2 路径白名单

写入路径必须匹配一期允许的右键菜单根：

- `Software\Classes\*\shell`
- `Software\Classes\AllFilesystemObjects\shell`
- `Software\Classes\Directory\shell`
- `Software\Classes\Directory\Background\shell`
- `Software\Classes\Drive\shell`
- `Software\Classes\Folder\shell`
- `Software\Classes\DesktopBackground\shell`
- `Software\Classes\SystemFileAssociations\...\shell`
- `Software\Classes\<ProgID>\shell`
- 上述场景的 `shellex\ContextMenuHandlers`
- `Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Blocked`
- `Software\Classes\CLSID\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}`

路径先规范化，拒绝 `..`、空段、非法字符和超长路径。

### 9.3 操作流程

1. 用户选择操作
2. 生成计划与变更预览
3. 校验路径、权限、旧值和冲突
4. 获取全局写入互斥锁
5. 保存精确注册表快照
6. 生成可读 `.reg` 备份
7. 提交操作
8. 重新读取并验证
9. 失败则回滚
10. 记录审计事件

### 9.4 备份位置

```text
%LOCALAPPDATA%\WinButler\
  backups\<timestamp>\manifest.json
  backups\<timestamp>\before.reg
  backups\<timestamp>\after.reg
  logs\audit.jsonl
```

备份不包含无关注册表分支，不包含用户凭据。

### 9.5 并发与外部变化

- 写入前比较预期旧值
- 发现外部变化时中止并要求重新扫描
- 同一用户会话使用命名互斥锁
- 不自动合并其他右键管理工具的并发修改

## 10. 启用与禁用策略

### 10.1 Windows 11 菜单样式

当前用户切换到经典样式：

- 创建 `HKCU\Software\Classes\CLSID\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}\InprocServer32`
- 将默认值设置为空字符串

恢复现代样式：

- 删除当前用户覆盖项

生效方式：

- 优先发送 Shell 变更通知
- 明确提示资源管理器重启需求
- 提供用户确认后的资源管理器重启

机器级切换不在一期默认功能内。若后续加入，必须单独实现全用户快照和回滚。

### 10.2 Shell 命令项

优先使用 `LegacyDisable` 值隐藏菜单项。启用时删除该值。

实施前必须验证：

- Windows 11 资源管理器是否遵循该值
- 对 `Extended` 和 `AppliesTo` 项是否有副作用
- 多级子菜单是否继承状态

### 10.3 传统 ShellEx

使用 Shell Extensions Blocked 列表：

- 当前用户：`HKCU\Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Blocked`
- 机器级：`HKLM\Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Blocked`

以 CLSID 作为值名。禁用时添加值，启用时删除值。必须保留其他软件已有的值。

### 10.4 Packaged COM

技术验证阶段比较以下候选机制：

1. HKCU Shell Extensions Blocked 列表
2. HKLM Shell Extensions Blocked 列表
3. 包级注册覆盖或策略
4. 不可安全禁用时降级为只读

验收标准不是“写入了某个键”，而是资源管理器重启后菜单项确实隐藏，且启用后确实恢复。

### 10.5 删除

一期删除仅允许：

- win-butler 自建项
- 明确标记为用户范围的 Shell 命令项
- 已完成备份并二次确认的项目

系统项、包项和机器级项默认不提供删除。

## 11. 添加与编辑菜单项

### 11.1 自建 Shell 命令项

默认写入 `HKCU\Software\Classes\<scene>\shell\<generated-id>`。

支持字段：

- 显示名称
- 图标
- 命令
- 扩展菜单
- 适用范围
- 自定义位置

### 11.2 命令安全

- 命令只保存为字符串
- 显示参数占位符解释：`%1`、`%V`、`%L`、`%W`
- 不自动执行用户输入命令
- 不通过 `cmd.exe /c` 拼接
- 保存前提示路径不存在、未加引号或包含危险字符的情况

### 11.3 编辑第三方项

一期默认只读。高级模式允许编辑时，必须先创建备份，并在变更预览中显示精确注册表差异。

## 12. UI 与交互

### 12.1 主界面结构

- 顶部：应用名称、扫描状态、刷新、搜索
- 左侧：场景导航
- 主区：菜单项表格
- 右侧或下方：详情面板
- 顶部状态区：Windows 11 菜单样式与生效状态

### 12.2 菜单样式控件

使用分段控件：

- 现代
- 经典
- 跟随系统或未覆盖

下方显示：

- 当前检测状态
- 是否需要重启资源管理器
- 当前用户范围
- 最近一次变更时间

### 12.3 菜单项表格

默认列：

- 名称
- 类型
- 场景
- 状态
- 范围
- 来源
- 发布者
- 风险

详情面板显示：

- 注册表路径
- CLSID
- 包名
- 文件路径
- 版本与签名
- 启用机制
- 能力与限制
- 诊断信息

### 12.4 操作

- 启用或禁用：开关或明确的文本操作
- 批量操作：选中后统一预览
- 定位注册表：只打开 Regedit 到对应路径
- 定位文件：只在文件存在时提供
- 复制信息：用于问题反馈
- 撤销：从历史记录恢复

### 12.5 状态与错误

必须覆盖：

- 首次扫描
- 扫描中
- 无结果
- 搜索无结果
- 权限不足
- 包清单损坏
- DLL 缺失
- 注册表被外部修改
- 回滚失败
- 资源管理器重启失败

### 12.6 EUI-NEO 使用方式

- `menuBar`：主菜单
- 侧边导航：场景切换
- `segmented`：样式切换
- `dataTable`：菜单项列表与多选
- `form`：详情与编辑
- `dialog`：确认、错误、变更预览
- `toast`：非阻塞结果提示
- `loader`：扫描和详情页状态

UI 状态由应用层对象拥有。扫描结果、选择、筛选和历史不放入临时 compose 局部变量。

### 12.7 视觉与可用性

- 默认窗口逻辑尺寸：1280 x 800
- 最小逻辑尺寸：1024 x 680
- 支持 100%、125%、150%、200% 缩放
- 键盘可完成扫描、搜索、切换场景、启停和撤销
- 长名称省略显示，详情面板显示完整值
- 不依赖颜色单独表达风险

## 13. 提权代理

### 13.1 为什么需要

机器级菜单项和部分包注册需要管理员权限。直接让整个 UI 提权会扩大攻击面，并让拖放、普通文件访问和 UI 自动化行为发生变化。

### 13.2 协议

- 仅本机命名管道
- 客户端校验父进程和用户 SID
- 请求使用固定 schema
- 只支持预定义操作 ID
- 服务端重新校验路径白名单
- 请求和响应带版本号、大小上限和超时
- 不传输任意 DLL、脚本或命令行

### 13.3 提权时机

只有提交机器级变更时启动代理。扫描和当前用户变更不需要提权。

## 14. 日志与隐私

- 本地审计日志默认保留 90 天
- 日志记录操作类型、时间、用户、路径、CLSID、结果
- 不记录完整环境变量、凭据、用户目录以外的文件内容
- 提供导出诊断包功能
- 诊断包不包含第三方二进制

## 15. 本机验证策略

验收以开发机上的实际构建和程序运行结果为准。不建设或要求端到端测试、浏览器/UI 自动化、虚拟机版本矩阵、自托管 runner 或新的测试框架。

### 15.1 必须完成

- 在本机按 README 配置并构建 `win_butler` 与 `win_butler_scan`
- 启动 `win_butler.exe`，确认主窗口正常显示、扫描可完成、搜索/筛选/详情可用
- 对本期交付的核心功能进行本机人工操作检查
- 涉及注册表写入时，优先使用现有可恢复 fixture 或当前用户范围；操作前备份，操作后检查并恢复
- 记录 Windows 版本/build、构建配置、启动结果、实际检查步骤及未验证权限/系统状态

### 15.2 可选检查

仓库已有的单元测试和 fixture 可在有助于定位问题时运行，但不是完成阶段或发布的门槛。只有能保护关键逻辑且适合现有结构时才补充窄范围测试；不得为了满足测试惯例新增测试目录、自动化 UI 或 VM 矩阵。

## 16. 本机构建与可选交付

### 16.1 本机构建

- 使用本机 Windows、CMake 和 MSVC 构建 Debug 或 Release
- 本机启动桌面应用并检查本期核心功能
- GitHub Actions、CI、测试报告和自托管 runner 均不是验收要求

### 16.2 可选发布物

如需要向其他机器分发，再生成：

- `win-butler-<version>-x86.zip`
- SHA-256 校验文件
- Release Notes 与已知限制

正式发布需要代码签名证书。没有证书时需明确标记为开发版。分发包和干净机器验证不是当前本机运行目标的门槛。

## 17. 里程碑

按一名熟悉 Windows Shell 与 C++ 的资深开发者估算，本文覆盖的右键菜单一期约 10 到 12 周。该估算不包含根目录全产品计划中的其他工具模块；技术验证结果可能调整后续工期。

### M0：技术验证与决策，1 周

交付：

- 传统 `shell`、`shellex`、Packaged COM 最小样本
- 样式切换注册表验证报告
- 传统 ShellEx Blocked 机制验证
- Packaged COM 禁用机制验证
- EUI-NEO Windows 构建样例
- 一期范围与风险确认

退出条件：

- Packaged COM 至少有一个可验证、可回滚的禁用路径，或明确决定降级为只读
- EUI-NEO 能在目标 Windows 11 版本上构建运行

### M1：工程骨架与只读目录，2 周

交付：

- CMake 工程与本机配置/构建
- EUI-NEO 主窗口
- 核心数据模型
- Shell 命令项扫描
- ShellEx 扫描
- 基础列表、搜索、筛选、详情
- 现有工程可在本机配置和构建

退出条件：

- 典型开发机可完成全量只读扫描
- 扫描期间 UI 不阻塞
- 错误项有诊断而不是丢失

### M2：Windows 11 样式管理，1 周

交付：

- 样式检测
- 当前用户现代/经典切换
- 变更预览
- 快照、验证、回滚
- 资源管理器重启入口

退出条件：

- 在当前开发机 Windows build 上切换并恢复
- 外部修改可被检测
- 回滚后注册表与操作前一致

### M3：传统菜单项管理，2 周

交付：

- `LegacyDisable` 启停
- ShellEx Blocked 启停
- 批量启停
- 撤销与审计
- 风险与权限提示

退出条件：

- 自建 Shell 命令项与 ShellEx 样本可稳定启停
- 不破坏已有 Blocked 值
- 失败路径能回滚

### M4：Packaged COM 管理，2 周

交付：

- 包枚举与清单解析
- `windows.fileExplorerContextMenus` 筛选
- CLSID、Server、包身份和文件路径关联
- 已验证的启停机制
- 包更新和卸载后的残留处理

退出条件：

- 现代菜单项可稳定识别
- 禁用与启用经过资源管理器重启验证
- 不支持安全写入的项明确标记为只读

### M5：可用性与安全加固，2 周

交付：

- 提权代理
- 自定义菜单项添加与编辑
- 扩展名、ProgID、新建、发送到、打开方式、WinX 只读盘点
- 中英文资源框架
- 完整错误状态和诊断导出
- 高 DPI 与键盘导航

退出条件：

- 机器级操作不要求整个 UI 提权
- 关键流程键盘可操作
- 诊断包不包含敏感数据

### M6：Beta 与发布，1 到 2 周

交付：

- 当前开发机的构建与运行检查记录
- 便携版 ZIP
- 用户文档
- 已知限制
- 发布签名与校验和

退出条件：

- 所有 P0 验收项通过
- 没有已知的高风险数据破坏问题
- 回滚和撤销在发布版本中可用

## 18. 工作分解

### 18.1 研究与基础设施

- WB-001 记录本机 Windows 版本、build 和构建工具链
- WB-002 核对已有传统 shell、ShellEx 与 Packaged COM fixture 的构建/清理方式
- WB-003 在本机确认经典/现代样式状态检测和恢复路径
- WB-004 在本机确认 Packaged COM 扫描与 Blocked 状态读取
- WB-005 验证经典/现代样式注册表切换
- WB-006 验证 Packaged COM 禁用机制
- WB-007 保持 CMake、EUI-NEO 的本机配置、构建和运行说明

### 18.2 Shell 识别

- WB-010 实现注册表路径模型与 Hive 扫描
- WB-011 实现 Shell 命令项解析
- WB-012 实现 ContextMenuHandlers 解析
- WB-013 实现 CLSID 与 Server 解析
- WB-014 实现文件元数据与签名信息读取
- WB-015 实现包枚举与 AppxManifest 读取
- WB-016 实现 Packaged COM 与清单关联
- WB-017 实现诊断与部分失败处理
- WB-018 实现扫描取消和性能基准

### 18.3 写入与安全

- WB-020 实现路径规范化和白名单
- WB-021 实现变更计划与旧值比较
- WB-022 实现精确快照和 `.reg` 备份
- WB-023 实现提交、验证和回滚
- WB-024 实现撤销历史
- WB-025 实现 Shell 命令项启停
- WB-026 实现 ShellEx Blocked 启停
- WB-027 实现 Packaged COM 启停
- WB-028 实现提权代理协议
- WB-029 实现提权代理服务端

### 18.4 UI

- WB-030 实现主窗口布局
- WB-031 实现场景导航
- WB-032 实现菜单项表格与筛选
- WB-033 实现详情面板
- WB-034 实现样式分段控件
- WB-035 实现变更预览对话框
- WB-036 实现错误、空状态和扫描状态
- WB-037 实现撤销与历史界面
- WB-038 实现高 DPI 和键盘导航
- WB-039 实现自定义菜单项表单

### 18.5 本机运行与可选交付

- WB-040 本机构建桌面应用与扫描器
- WB-041 本机启动并人工检查扫描、搜索、筛选和详情
- WB-042 对当前期可逆的系统变更检查备份和恢复
- WB-043 本机检查失败提示、权限错误与日志
- WB-044 诊断包与日志
- WB-045 可选便携版打包
- WB-046 可选发布包验证
- WB-047 用户文档与已知限制
- WB-048 可选发布签名和校验和

## 19. 验收标准

### 19.1 功能

- 能准确识别当前 Windows 11 菜单样式
- 能在当前用户范围切换样式并恢复
- 能识别文件、文件夹、文件夹背景、驱动器场景的传统菜单项
- 能识别传统 `shellex` 项并解析 CLSID 与文件路径
- 能识别声明了 `windows.fileExplorerContextMenus` 的 Packaged COM 项
- 能禁用和启用已验证支持的传统项
- 能对已验证支持的 Packaged COM 项禁用和启用
- 能显示不可写项的原因

### 19.2 安全

- 所有写入有路径白名单
- 所有写入有快照和备份
- 失败自动回滚
- 成功后可以撤销
- 不加载第三方 Shell 扩展做扫描
- 不执行菜单命令
- UI 默认不提权
- 提权代理不接受任意命令

### 19.3 性能与稳定性

- 常规开发机冷扫描小于 2 秒
- UI 扫描期间可交互
- 取消扫描在 100 毫秒内响应
- 单个损坏项不导致全量失败
- 连续 20 次启停和撤销无残留或崩溃
- 资源管理器重启后状态一致

### 19.4 质量门禁

- 与本机使用配置一致的构建成功
- 桌面应用在当前开发机正常启动并完成本期核心操作
- 系统写入路径完成备份/恢复检查或明确标记为未开放
- 已知限制写入发布说明

## 20. 主要风险

| 风险 | 影响 | 应对 |
| --- | --- | --- |
| Packaged COM 没有公开稳定的禁用机制 | 现代菜单项只能只读 | M0 先验证，必要时调整一期承诺 |
| Windows 版本行为差异 | 同一注册表操作在不同版本表现不同 | 在当前开发机按 build 检测能力；未验证版本只读处理，不把多版本 VM 矩阵设为门槛 |
| 第三方 Shell 扩展导致资源管理器崩溃 | 用户环境不稳定 | 扫描不加载 DLL，禁用前备份，提供恢复入口 |
| 提权代理扩大攻击面 | 安全风险 | 固定协议、父进程校验、路径白名单 |
| EUI-NEO 原生窗口集成限制 | 托盘、提权、焦点等体验受限 | M0 做集成样例，系统能力通过原生层补齐 |
| GPL 参考代码污染 | 许可证风险 | 不复制源码，只记录独立验证的机制 |
| 注册表并发修改 | 覆盖其他工具设置 | 旧值比较、互斥锁、冲突中止 |
| 删除系统项造成不可逆损失 | 高 | 一期默认禁用，删除仅限自建项 |

## 21. 已确认决策

- 开源许可证：MIT
- 界面与文档：中英文双语
- 发布形式：仅便携版 ZIP，不提供安装程序
- 架构：仅 x86 32 位，暂不支持 ARM64
- 高级用户：允许编辑第三方菜单命令，但必须经过备份、预览和确认

## 22. 剩余待确认

- Packaged COM 禁用验证失败时的产品降级方式

## 23. 下一步

1. 在当前开发机检查 Packaged COM 安装状态、扫描、Blocked 状态读取和恢复路径
2. 完成菜单样式检测与当前用户经典/现代切换
3. 实现注册表事务、快照、回滚和撤销
4. 实现传统 shell 与 ShellEx 的启用和禁用
5. 扩展文件类型、ProgID、新建、发送到、打开方式和 WinX 场景
