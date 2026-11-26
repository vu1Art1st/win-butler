# win-butler

[English](README.en.md)

win-butler 是一个面向 Windows 的系统管理工具。一期聚焦资源管理器右键菜单管理。

## 当前状态

M0 技术验证和首个只读扫描演示已经完成：

- 32 位 Win32 扫描器可识别传统 `shell` 动词、`shellex` ContextMenuHandlers 和 Win11 Packaged COM
- EUI-NEO 桌面应用可运行，支持中英文切换、场景筛选、搜索和详情查看
- 已提供可选本地诊断夹具：传统 Shell 项、ShellEx DLL、Packaged COM MSIX
- 当前只读扫描在本机约 60 到 140 ms，结果按菜单项聚合
- 当前版本只读，不提供注册表写入按钮

## 一期目标

- Windows 11 现代右键菜单与经典右键菜单样式切换
- 文件、文件夹、文件夹背景、驱动器等场景的菜单项盘点
- 传统 `shell` 动词与 `shellex` ContextMenuHandlers 管理
- Windows 11 Packaged COM 右键扩展识别与管理
- 高级用户编辑第三方菜单命令
- 可回滚的注册表变更、变更预览和本地审计记录

## 技术方向

- Shell 层：纯 C++，直接使用 Win32、COM、注册表和 Windows 包 API
- UI：EUI-NEO `v0.6.5`
- 构建：CMake 3.24+、MSVC 2022 或更新版本
- 目标：Windows 11 x86，32 位进程
- 交付：便携版 ZIP，不提供安装程序
- 许可证：MIT

UI 进程默认不提权。机器级写入将由受控提权代理执行白名单操作。

## 构建

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
cmake --build build --config Release --parallel
```

运行桌面应用：

```powershell
.\build\Release\win_butler.exe
```

运行只读扫描器：

```powershell
.\build\Release\win_butler_scan.exe --summary
.\build\Release\win_butler_scan.exe
```

可选：运行仓库已有的单元测试（不是本机验收门槛）：

```powershell
ctest --test-dir build -C Release --output-on-failure
```

## 测试夹具

- `fixtures/classic-shell`：传统 shell 动词注册
- `fixtures/shellex`：可构建的 32 位 `IContextMenu` DLL
- `fixtures/packaged-com`：可构建、签名并显式安装的 64 位 Packaged COM MSIX

夹具只用于可恢复的本机诊断。Packaged COM 夹具默认只构建和签名，不安装；安装需用户明确操作。

## 文档

- [开发规则](AGENTS.md)
- [全产品开发方案与逐期 AI 提示词](docs/DEVELOPMENT_PLAN.md)
- [上下文菜单技术设计](docs/CONTEXT_MENU_TECHNICAL_DESIGN.md)
- [M0 技术验证记录](docs/M0_TECHNICAL_VALIDATION.md)
- [参考软件与许可证边界](docs/REFERENCE_ANALYSIS.md)

## 参考项目

- [Sordum Windows 11 Classic Context Menu](https://www.sordum.org/14479/windows-11-classic-context-menu-v1-3/)
- [BluePointLilac ContextMenuManager](https://github.com/BluePointLilac/ContextMenuManager)

参考项目只用于功能对照和公开机制研究。win-butler 不复制 ContextMenuManager 的 GPL-3.0 源代码。

## 许可证

[MIT](LICENSE)
