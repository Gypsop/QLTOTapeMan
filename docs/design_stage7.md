# 阶段 7：CLI 对齐与参数兼容（规划）

目标：保持与原项目相同的 CLI 参数体验，路由到对应 UI/功能；未实现功能给出友好提示。仅文档。

## 参考源（原项目）
- `ApplicationEvents.vb`：命令行解析与 MainForm 路由，参数：`-t`, `-s`, `-f`, `-c`, `-l`, `-rb`, `-wb`, `-raw`, `-mkltfs`。
- `LtfsCommand/` (C 部分)：脚本生成、raw 复制等命令行行为，可作为未来实现参考。

## 目标行为
- `qltotapeman -c` : 打开 Configurator（主窗口）。
- `qltotapeman -t <drive>` : 直接进入 LTFSWriter，在线模式，指定驱动路径。
- `qltotapeman -t <drive> -s` : 直接进入 LTFSWriter，离线模式（不读索引或仅模拟）；`ExtraPartitionCount = 1` 对齐原逻辑。
- `qltotapeman -f <schema>` : 离线加载 schema 进入 LTFSWriter 只读/预览。
- `qltotapeman -l` : 预留 Changer 工具（未实现则提示）。
- `qltotapeman -rb/-wb/-raw/-mkltfs` : 如未完成，对用户提示“暂未实现/待后续版本”。

## 设计要点
- 解析库：轻量（可手写或用 Qt 的 `QCommandLineParser`）。
- 路由：参数解析后创建对应窗口/模式；无参数时默认 Configurator。
- 平台权限：在需要管理员/root 时提前检测并提示（结合阶段 8 的提示策略）。
- 日志输出：控制台模式下打印关键状态/错误。

## 兼容性说明
- 对未实现命令保持占位，输出清晰的错误码与文案，避免静默失败。
