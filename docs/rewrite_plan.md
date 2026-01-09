# QLTOTapeMan（Qt6/C++）重构规划 — 详细步骤

目标：在 Qt6/C++ 中完整复刻 LTFSCopyGUI 的“直接读写”体验（含独立文件浏览器与预览写入逻辑），并抽象跨平台磁带/SCOSI/LTFS 操作，兼容 Win/Linux/macOS，支持多编译器（MSVC/MinGW/Clang/GCC）。不使用 QML。

## 阶段 0：总体设计与接口抽象
- **输出物**：接口草图 / 类图 / CMake 选项草案
- **参考文件**：
  - `LTFSCopyGUI/ApplicationEvents.vb`：入口参数解析（`-t/-s/-f/-c/-l`）与主窗体路由
  - `LTFSCopyGUI/TapeUtils.vb`：核心 SCSI/驱动封装、枚举、mkltfs、LogSense、MAM、读写块
  - `LTFSCopyGUI/LTFSWriter.vb`：直接读写主流程、状态机、UI 交互入口
  - `LTFSCopyGUI/FileDataProvider.vb`：写入管线与背压设计
- **设计要点**：
  - 抽象 `TapeDevice` 接口（open/close/read_block/write_block/write_filemark/read_position/log_sense/load/eject/mkltfs）。
  - 平台实现：`TapeDeviceWin`(DeviceIoControl + SPTD/SCSI_PASS_THROUGH)，`TapeDeviceSg`(Linux sg_io)，`TapeDeviceMac`(IOKit/SG user client)。
  - 高层服务 `LtfsService`：封装标签读取/索引读写、容量刷新、错误日志、额外分区处理、离线模式仿真。
  - 数据模型：ltfslabel/ltfsindex 解析与生成（移植自 `ltfsindex` 相关定义）。

## 阶段 1：项目骨架与构建系统
- **输出物**：根级 CMakeLists、`src/` 目录结构、`third_party/` 占位、Qt6 依赖声明
- **参考文件**：无（新建），但命名/模块划分需对齐上面接口设计
- **要点**：
  - 目标最低 Qt 6.0，组件：Widgets/Network/Concurrent。
  - 选项：`QLTOTAPEMAN_BUILD_TESTS`，`QLTOTAPEMAN_USE_SYSTEM_BLAKE3`（后续加密/校验可选）。
  - 安装忽略 QML，全部使用 `.ui`。

## 阶段 2：基础模型与工具层
- **输出物**：
  - `core/ltfs_index.*`：ltfsindex 结构、序列化/反序列化、排序（参考 `ltfsindex` 类型定义与 `LTFSWriter` 中对 schema 的访问）。
  - `core/ltfs_label.*`：卷标签与分区信息（参考 `TapeUtils.mkltfs` 参数与 `LTFSWriter` 对 blocksize/index partition 的使用）。
  - `core/hash_zstd.*`：Blake3/可选 Zstd（参考 `LTFSWriter` 的 Blake3/Zstd 引用，按需 stub）。
- **参考文件**：
  - `LTFSCopyGUI/LTFSWriter.vb`（索引读写、属性字段、xattr 处理、`LoadIndexFile`）。
  - `LTFSCopyGUI/TapeUtils.vb`（label 结构、mkltfs 参数、MAM 读写）。

## 阶段 3：设备抽象与平台实现
- **输出物**：`io/tape_device.h/.cpp`（接口）；`io/tape_device_win.cpp`、`io/tape_device_sg.cpp`、`io/tape_device_mac.cpp` 占位
- **参考文件**：
  - `TapeUtils.vb` 中的：
    - 驱动列表获取 `GetTapeDriveList`（枚举、序列号、DevIndex）
    - 打开/关闭/读写 `OpenTapeDrive`、`ReadBlock`、`Write`、`WriteFileMark`、`LoadEject`、`ReadPosition`、`LogSense`、`MAMAttribute`、`mkltfs`
    - Sense 解析、Block size/DriverType 处理、`SCSIOperationLock`
  - `LTFSConfigurator.vb` 中对驱动的操作按钮逻辑（装载、退带、加载方式）
- **要点**：
  - Windows: DeviceIoControl(SCSI_PASS_THROUGH(_DIRECT))；路径兼容 `\\.\TAPE*` 与 `\\.\GLOBALROOT\Device\...`。
  - Linux: `/dev/sg*` + `SG_IO`，兼容 `/dev/nst*` 只写；需要 root / CAP_SYS_RAWIO 检查提示。
  - macOS: IOKit SCSI/StorageFamily；若困难，可先 stub，提示仅 Linux/Windows 支持。
  - 线程安全：仿照 `TapeUtils.SCSIOperationLock`。

## 阶段 4：高层服务与任务流
- **输出物**：`core/ltfs_service.*`
- **参考文件**：
  - `LTFSWriter.vb`：
    - 在线读取索引：`LoadLTFS`/`LoadIndexFile`（读取、解析、填充 `schema`，区分 Index/Data 分区）
    - 写入流程：`WriteSelected` 系列、`WriteCurrentIndex`、`AutoDump`、`CapacityRefresh`、`SetStatusLight`
    - 速度/限速、HashOnWrite、自动索引间隔、自动清洁计数、EOD 判断、暂停/继续、Flush、Eject、状态灯枚举 `LWStatus`
  - `FileDataProvider.vb`：背压与文件预读策略
  - `TapeCopy.vb`：块级复制/文件标记处理/EOD 检测
- **要点**：
  - 将 UI 状态更新抽离为信号/回调（Qt signals）。
  - 统一错误与 sense 解析输出（文本 + 代码）。
  - 离线模式：不触碰设备，支持外部 schema 预览。

## 阶段 5：写入管线与缓存
- **输出物**：`pipeline/file_data_provider.*`（Qt 版）
- **参考文件**：`FileDataProvider.vb`（Pipe + RingBuffer + 小文件缓存 + requireSignal 模式）、`LTFSWriter.FileRecord`（文件元数据、预读/xattr 处理、并发打开、重试提示）。
- **要点**：
  - 保持“小文件缓存 + 背压”的策略；Qt 版可用 `QIODevice` + 自实现环形缓冲。
  - 限速：在写入循环中结合时间窗口计算，复刻 `SpeedLimit` 逻辑。
  - 哈希：Blake3 可选；允许关闭。

## 阶段 6：UI 复刻（Qt Designer `.ui`）
- **输出物**：
  - `ui/MainWindow`（对应 LTFSConfigurator 主界面关键功能：驱动列表、装载/退带、调试日志、切换到直接读写按钮）。
  - `ui/LTFSWriterWindow`（“直接读写”窗口）：
    - 左侧树/右侧列表文件管理器（复刻 `FileBrowser.vb` + `LTFSWriter` 文件区）
    - 状态栏：条码、分区、块大小、当前位置、容量、速度/限速、剩余时间、状态灯
    - 控制区：离线/在线、索引写入周期、哈希、限速、自动清洁、自动导出索引、加密密钥/密码、额外分区
    - 日志/进度：文本日志、进度条、暂停/继续/停止、自动 Flush
  - `ui/FileBrowserDialog`：独立树形选择器，支持全选/按大小/正则筛选。
- **参考文件**：
  - `FileBrowser.vb`（树形选择、复选状态联动、筛选菜单）
  - `LTFSWriter.vb` UI 事件与菜单（`ToolStripMenuItem` 系列、右键操作、状态灯更新）
  - `LTFSConfigurator.vb` 触发 `LTFSWriter` 的入口按钮
- **要点**：
  - 保留原有对话框和弹窗流程（确认/错误/进度提示）。
  - 翻译资源：用 Qt 翻译（`zh`, `en`, `zh_Hant`）。

## 阶段 7：CLI 对齐与参数兼容
- **输出物**：Qt/C++ CLI 入口，支持：`-t <drive>`、`-s`(offline)、`-f <schema>`、`-c`(配置器)、`-l`(Changer/待定)、`-rb/-wb/-raw/-mkltfs`（视后续实现）
- **参考文件**：`ApplicationEvents.vb` 参数解析与分支；`LtfsCommand/` C 部分可参考 CLI 行为。
- **要点**：保持与原项目的使用习惯，未实现的子命令需给出友好提示。

## 阶段 8：平台特性与权限处理
- **输出物**：
  - 权限检测与提示模块（root/UAC/pkexec）
  - 日志与诊断（类似 `TextBoxDebugOutput`，可保存 log 文件）
- **参考文件**：
  - `ApplicationEvents.vb` UAC 检查/自提权
  - `LTFSConfigurator.vb` 调试面板（SCSI 直发、LogSense 导出）

## 阶段 9：测试与验证
- **输出物**：
  - 最小设备模拟/回环测试（可在无磁带机环境下编译运行 UI 与逻辑；I/O 部分用 stub）
  - 单测/集成测试结构（可选 GoogleTest / Qt Test）
- **参考文件**：以 `TapeUtils` 行为为基准写用例（Sense 解析、索引读写、管线限速计算）。

## 阶段 10：文档与发布
- **输出物**：
  - 使用说明（GUI/CLI）
  - 构建说明（Win MSVC/MinGW，Linux GCC/Clang，macOS Clang）
  - 依赖列表与可选组件（Blake3/Zstd）

---

## 关键功能映射表（快速索引）
- **驱动管理/装载**：`LTFSConfigurator.vb`（按钮事件：LoadThreaded/Unthread/Eject/Mount/Assign/UnMap；调用 `TapeUtils.LoadEject/Map/UnMap/Mount`）
- **直接读写入口**：`LTFSConfigurator.vb` → `ButtonLTFSWriter_Click` 启动 `LTFSWriter`
- **索引加载/保存**：`LTFSWriter.vb` `LoadIndexFile`、`WriteCurrentIndex`、`AutoDump`
- **文件选择器**：`FileBrowser.vb`（树形递归、全选/筛选/正则、勾选联动）
- **写入管线**：`FileDataProvider.vb` + `LTFSWriter.FileRecord` + `LTFSWriter` 中的写入循环（限速/哈希/背压/小文件缓存/预读）
- **块级复制**：`TapeCopy.vb`（读块→写块/文件标记，EOD 检测，自动 Flush）
- **SCSI/设备操作**：`TapeUtils.vb`（核心：Open/Close/Read/Write/WriteFM/Flush/ReadPosition/LogSense/MAM/mkltfs，设备枚举、Sense 解析）
- **调试/日志**：`LTFSConfigurator.vb` 调试面板；`LTFSWriter` 状态灯与日志输出；`ApplicationEvents.vb` 控制台模式。

## 里程碑与优先级
1) **最小可运行 UI + 设备 stub**：完成骨架与“直接读写”窗口交互，离线加载 schema。 
2) **Linux sg_io 路径读写**：实现读取 label/index，完成基本写入循环（无限速/哈希）。
3) **完整写入功能**：限速、哈希、索引周期写入、容量刷新、自动导出。
4) **Windows 支持**：完成 DeviceIoControl 实现与 UAC 提示。
5) **macOS（可选）**：实现 IOKit 版或声明暂不支持。
6) **发布与文档**：构建脚本、翻译、使用说明。

## 后续行动（建议）
- 先落地 CMake + 目录骨架 + 接口头文件，占位实现（stub）。
- 设计 `.ui` 原型（Main、LTFSWriter、FileBrowser）。
- 实现 Linux sg_io 版 TapeDevice，跑通离线/在线索引加载与最小写入循环。
- 逐步移植功能细节（限速、哈希、自动索引/容量刷新、日志/状态灯）。
