# 阶段 1：项目骨架与构建系统（规划）

> 目标：生成可跨平台的 Qt6/C++ 构建骨架与目录布局，但本阶段只做文档设计，不落地代码。

## 1. 目录规划（与阶段 0 接口对应）
```
QLTOTapeMan/
├─ CMakeLists.txt              # 顶层，Qt6 依赖与全局选项
├─ src/
│  ├─ app/                     # main.cpp, cli 解析与路由
│  ├─ core/                    # ltfs_label/index/service 等实现
│  ├─ io/                      # tape_device 抽象 + 平台实现 stub
│  ├─ pipeline/                # file_record / file_data_provider
│  ├─ ui/                      # Qt Designer .ui, 后续阶段填充
│  └─ CMakeLists.txt           # 目标与子目录聚合
├─ third_party/                # 可选依赖（Blake3/Zstd 等），先留空
├─ docs/                       # 规划文档（包括 stage0/1）
└─ cmake/                      # 工具宏（如 Qt wrap_ui/moc 辅助），可后续添加
```

## 2. 顶层 CMake 设计要点
- 最低 Qt 版本：`find_package(Qt6 6.0 COMPONENTS Widgets Network Concurrent REQUIRED)`
- 选项（与阶段 0 对齐）：
  - `QLTOTAPEMAN_BUILD_TESTS` (default OFF)
  - `QLTOTAPEMAN_USE_SYSTEM_BLAKE3` (OFF)
  - `QLTOTAPEMAN_ENABLE_ZSTD` (OFF)
  - `QLTOTAPEMAN_WITH_WIN` (ON if WIN32)
  - `QLTOTAPEMAN_WITH_LINUX` (ON if UNIX AND NOT APPLE)
  - `QLTOTAPEMAN_WITH_MAC` (ON if APPLE)
- 语言标准：`set(CMAKE_CXX_STANDARD 20)`，`CMAKE_POSITION_INDEPENDENT_CODE ON`
- 编译警告：GCC/Clang `-Wall -Wextra -Wpedantic`；MSVC `/W4 /permissive-`
- 生成目标：
  - 主可执行 `qltotapeman`（链接 Qt6 Widgets/Network/Concurrent）
  - 可选：tests（后续阶段）
- moc/uic/rcc：使用 `qt6_wrap_ui` / `Qt6::Widgets` 自动处理；不引入 QML。

## 3. 子目录 CMake（src/CMakeLists）
- 分目标静态库或对象库：
  - `qlto_core`    -> `core/*.cpp`
  - `qlto_io`      -> `io/*.cpp`（平台 stub 受选项控制）
  - `qlto_pipeline`-> `pipeline/*.cpp`
  - `qlto_ui`      -> `ui` 生成的代码（.ui -> .h/.cpp）
- 主可执行链接顺序：`qltotapeman` 链接上述模块与 Qt6 库。
- 平台宏：
  - `QLTO_PLATFORM_WIN`, `QLTO_PLATFORM_LINUX`, `QLTO_PLATFORM_MAC` 根据选项定义。
  - 若禁用某平台实现，则只编译通用 stub，运行时报不支持。

## 4. 预留/占位文件清单（空实现即可，后续填充）
- `src/app/main.cpp`
- `src/core/ltfs_label.{h,cpp}`
- `src/core/ltfs_index.{h,cpp}`
- `src/core/ltfs_service.{h,cpp}`
- `src/io/tape_device.h`
- `src/io/tape_device_win.cpp` (受 `QLTOTAPEMAN_WITH_WIN`)
- `src/io/tape_device_sg.cpp`  (受 `QLTOTAPEMAN_WITH_LINUX`)
- `src/io/tape_device_mac.cpp` (受 `QLTOTAPEMAN_WITH_MAC`)
- `src/io/tape_enumerator.h`
- `src/pipeline/file_record.{h,cpp}`
- `src/pipeline/file_data_provider.{h,cpp}`
- `src/ui/` 下 .ui 原型（后续阶段 6 填充）
- `src/CMakeLists.txt`（列出目标、源文件、条件编译）

## 5. 平台条件与占位逻辑
- 当平台实现未启用时，对应源文件不编译；`TapeDevice` 工厂返回“此平台未支持”错误。
- 允许仅构建 Linux 或 Windows，以便 CI / 本地分别验证。

## 6. 参考映射
- 构建层无直接参考代码；接口与命名对齐 `docs/design_stage0.md`（从 `TapeUtils.vb`、`LTFSWriter.vb` 抽象而来）。

## 7. 后续动作（需指令后执行）
- 按此文档生成 `CMakeLists.txt` 及空源文件/目录。
- 确认平台开关与最小可编译骨架后，再进入阶段 2 模型实现规划/落地。
