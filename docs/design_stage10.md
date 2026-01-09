# 阶段 10：文档与发布（规划）

目标：输出最终用户/开发者文档、构建说明、发布策略；仅文档规划。

## 文档清单
- **用户指南**：
  - 安装依赖（Qt6+编译器，平台权限要求）。
  - GUI 使用：Configurator（装载/挂载/映射）、LTFSWriter（直接读写流程、离线/在线模式、加密/限速/索引周期、自动导出）、FileBrowser 选择。
  - CLI 用法：`-t/-s/-f/-c/-l/-rb/-wb/-raw/-mkltfs` 状态说明，未实现项说明。
- **开发者指南**：
  - 目录与模块说明（core/io/pipeline/ui/app）。
  - CMake 选项（平台开关、哈希/压缩可选项、测试开关）。
  - 平台后端约束（Win DeviceIoControl、Linux sg_io、macOS IOKit/stub）。
  - 贡献流程与代码风格（Qt/C++20、无 QML、命名约定）。
- **故障排查**：
  - 权限不足、设备不可见、命令超时、sense key 常见解释。
  - 构建失败（Qt 版本、编译器不兼容）。

## 发布/打包
- 源码发布：GitHub Release（含依赖说明），默认不附带第三方二进制。
- 二进制（可选）策略：
  - Windows：MSVC/MinGW 构建；附带 Qt 依赖说明或静态/动态选项。
  - Linux：AppImage/包管理待定；至少提供构建脚本。
  - macOS：若后端未实现，需在 Release 说明中标记“macOS 后端受限/暂不支持”。

## 版本与兼容
- 最低 Qt 6.0；不支持 Qt5。
- 平台后端可选择性关闭，发布说明需列出启用的后端。

## 后续维护
- 更新日志（CHANGELOG）；
- 已知问题与 TODO；
- 测试矩阵（与阶段 9 CI 对齐）。
