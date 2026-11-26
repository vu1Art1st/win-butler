# win-butler

win-butler 是一个面向 Windows 的系统管理工具。一期聚焦资源管理器右键菜单管理：

- Windows 11 现代右键菜单与经典右键菜单样式切换
- 文件、文件夹、文件夹背景、驱动器等场景的菜单项盘点
- 传统 `shell` 动词与 `shellex` ContextMenuHandlers 管理
- Windows 11 Packaged COM 右键扩展识别与管理
- 可回滚的注册表变更、变更预览与本地审计记录

## 技术方向

- Shell 识别与管理层：纯 C++，直接使用 Win32、COM、注册表和 Windows 包 API
- UI 层：EUI-NEO
- 构建：CMake + MSVC 2022
- 一期目标平台：Windows 11 x64 22H2 及以上

UI 进程默认不提权。需要修改 `HKLM` 或机器级包注册时，通过受控提权代理执行白名单操作。

## 项目状态

当前处于规划与技术验证阶段，尚未进入功能实现。

详细计划见 [docs/DEVELOPMENT_PLAN.md](docs/DEVELOPMENT_PLAN.md)。

参考样本分析与许可证边界见 [docs/REFERENCE_ANALYSIS.md](docs/REFERENCE_ANALYSIS.md)。

## 安全原则

- 扫描默认只读，不激活第三方 COM 服务器
- 禁用优先于删除
- 写入前保存精确快照和可读备份
- 写入失败自动回滚，成功后提供撤销
- 所有注册表路径经过规范化与白名单校验
- 不执行从菜单项中读取的任意命令

## 功能参考

- [Sordum Windows 11 Classic Context Menu](https://www.sordum.org/14479/windows-11-classic-context-menu-v1-3/)
- [BluePointLilac ContextMenuManager](https://github.com/BluePointLilac/ContextMenuManager)

参考项目只用于功能对照和公开机制研究。ContextMenuManager 使用 GPL-3.0，win-butler 不复制其源代码，除非项目未来明确采用兼容许可证。

## 许可证

项目许可证尚未确定。在确定之前，仓库保持私有。
