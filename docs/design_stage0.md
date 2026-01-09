# 阶段 0：总体设计与接口抽象（Qt6/C++)

> 依据 `docs/rewrite_plan.md`，形成可落地的接口/类草案与构建选项草案。禁止修改 `rewrite_plan.md`。

## 1. 模块分层概览
- **core/**: 纯业务与数据模型（无平台依赖）。
  - `ltfs_label`: 卷标签、分区号、块大小、额外分区、加密参数。
  - `ltfs_index`: 目录树/文件元数据、xattr、序列化/反序列化、排序/选择状态。
  - `ltfs_service`: 结合设备接口完成 label/index 读写、容量/位置查询、索引周期写入等高层操作。
  - `hash/`：Blake3（可选）与 CRC/MD5/SHA 暂留接口；压缩 Zstd（可选）。
- **io/**: 平台相关的 SCSI/设备访问。
  - 抽象接口 `TapeDevice` + 平台实现 `TapeDeviceWin` / `TapeDeviceSg` / `TapeDeviceMac`。
  - 设备发现 `TapeEnumerator`，输出统一的 `BlockDevice` 结构。
- **pipeline/**: 写入/读出数据管线。
  - `FileDataProvider`：小文件缓存 + 背压环形缓冲 + 限速钩子。
  - `FileRecord`：文件元数据、预读、xattr 读写。
- **ui/**: Qt Widgets + .ui，封装成窗口/对话框（后续阶段实现，现仅接口约定）。
- **app/**: CLI 入口、启动路由（配置器 / 直接读写 / 更换器等）。

## 2. 核心接口草案

### 2.1 TapeDevice（io/tape_device.h）
```cpp
struct SenseData { std::array<uint8_t, 64> buf; };
struct PositionData { uint64_t block; uint32_t partition; /* add fields as needed */ };
struct LogPage { uint8_t page; std::vector<uint8_t> data; };
struct MamAttribute { uint8_t id; std::vector<uint8_t> data; };

class TapeDevice {
public:
    virtual ~TapeDevice() = default;
    virtual bool open(const std::string& path, std::string& err) = 0;
    virtual void close() = 0;
    virtual bool read_block(std::vector<uint8_t>& out, uint32_t block_len, SenseData& sense, std::string& err) = 0;
    virtual bool write_block(const uint8_t* data, size_t len, SenseData& sense, std::string& err) = 0;
    virtual bool write_filemark(SenseData& sense, std::string& err) = 0;
    virtual bool load(bool threaded, SenseData& sense, std::string& err) = 0;
    virtual bool unload(SenseData& sense, std::string& err) = 0;
    virtual bool read_position(PositionData& pos, SenseData& sense, std::string& err) = 0;
    virtual bool log_sense(uint8_t page, uint8_t subpage, LogPage& out, SenseData& sense, std::string& err) = 0;
    virtual bool read_mam(uint8_t page, uint8_t id, MamAttribute& out, SenseData& sense, std::string& err) = 0;
    virtual bool write_mam(uint8_t page, const MamAttribute& in, SenseData& sense, std::string& err) = 0;
    virtual bool format_mkltfs(/* label info, partitions, block size, encryption, etc. */ std::string& err) = 0;
    // Optional: raw SCSI passthrough for debug
    virtual bool scsi_pass_through(const std::vector<uint8_t>& cdb,
                                   std::vector<uint8_t>& data,
                                   bool data_in,
                                   uint32_t timeout_ms,
                                   SenseData& sense,
                                   std::string& err) = 0;
};
```
**对应参考**：`TapeUtils.vb` (`OpenTapeDrive`, `ReadBlock`, `Write`, `WriteFileMark`, `LoadEject`, `ReadPosition`, `LogSense`, `MAMAttribute`, `mkltfs`, `IOCtlDirect`, `SCSIOperationLock`).

### 2.2 TapeEnumerator（io/tape_enumerator.h）
```cpp
struct BlockDevice {
    std::string vendor;
    std::string product;
    std::string serial;
    std::string device_path;   // e.g., "\\\\.\\TAPE0" or "/dev/sg2"
    std::string display_name;  // friendly
    std::string os_driver;     // Win: driver desc; Linux: sg/nst; macOS: IOService name
    int index = -1;            // DevIndex / order
};

class TapeEnumerator {
public:
    virtual ~TapeEnumerator() = default;
    virtual std::vector<BlockDevice> list(std::string& err) = 0;
};
```
**对应参考**：`TapeUtils.GetTapeDriveList`、`LTFSConfigurator` 列表显示/序列号映射。

### 2.3 LTFS 数据模型（core/ltfs_label.h, core/ltfs_index.h）
- Label：`blocksize`, `partitions`(index/data/extra), `barcode`, `volumelabel`, `generation`, `encryption key info`, `worm`。
- Index：目录/文件（name, uid, length, times, xattr, selected），分区位置 (`location.partition`, `startblock`)，写入状态（未写、已写）。
- 方法：序列化/反序列化（对应 `.schema`）、排序、选择标记（FileBrowser 勾选）、xattr 读写。
**对应参考**：`ltfsindex` 类型定义、`LTFSWriter.LoadIndexFile/WriteCurrentIndex/AutoDump`、`ltfslabel` 数据在 `TapeUtils.mkltfs` 参数中出现。

### 2.4 LtfsService（core/ltfs_service.h）
职责：基于 `TapeDevice` 完成高层操作，向 UI 提供信号/回调。
```cpp
class LtfsService {
public:
    struct Callbacks {
        std::function<void(const std::string&)> log;
        std::function<void(double)> progress; // 0~1
        std::function<void(const std::string&)> status;
        std::function<void(const SenseData&)> sense;
    };

    bool attach_device(std::unique_ptr<TapeDevice> dev);
    bool load_label(Label& out, std::string& err);           // 读卷标签
    bool load_index(Index& out, bool offline, std::string& err); // 读索引或离线加载
    bool write_files(const std::vector<FileRecord>& files, const WriteOptions&, std::string& err);
    bool write_index(bool force, std::string& err);
    bool refresh_capacity(CapacityInfo&, std::string& err);
    bool eject(std::string& err);
    // ...
};
```
- `WriteOptions` 包含：块长、哈希开关、限速、索引周期、自动清洁阈值、离线模式、额外分区、加密密钥。
- `FileRecord` 与 `FileDataProvider` 协同（见下）。
**对应参考**：`LTFSWriter` 里的主流程（加载、写入、索引周期、容量刷新、状态灯、自动导出）。

### 2.5 FileDataProvider + FileRecord（pipeline/）
- `FileRecord`：源路径、ltfsindex.file、预读缓存、xattr 读取、Open/Close/ReadAllBytes，错误重试策略。
- `FileDataProvider`：
  - 小文件缓存阈值（默认 16 KiB，参考 `FileDataProvider.vb`）。
  - 背压环形缓冲（默认 256 MiB）。
  - requireSignal 模式（按需推进）/积极模式。
  - 限速钩子（时间片+累计字节）。
**对应参考**：`FileDataProvider.vb`、`LTFSWriter.FileRecord`、`LTFSWriter` 写入循环。

### 2.6 状态与事件
- `LWStatus`（枚举）：NotReady/Busy/Succ/Err/Pause 等，供 UI 灯/进度条使用。
- 回调/信号：日志输出、进度更新、剩余时间估算（由 `LtfsService` 提供原始数据，UI 计算）。
**对应参考**：`LTFSWriter` 中的 `SetStatusLight`、状态栏文本更新。

## 3. CLI 入口（app/main.cpp）
- 支持参数：`-t <drive>`，`-s`（离线）、`-f <schema>`、`-c`（配置器 UI）、`-l`（Changer/待定）、`-rb/-wb/-raw/-mkltfs`（未实现则提示）。
- 解析后路由到：配置器窗口 / 直接读写窗口 / 索引预览（离线）。
**对应参考**：`ApplicationEvents.vb` 中的参数分支与 UAC 检查。

## 4. CMake 选项草案
- 最低要求：Qt ≥ 6.0，模块 Widgets, Network, Concurrent。
- 选项：
  - `QLTOTAPEMAN_BUILD_TESTS` (ON/OFF)
  - `QLTOTAPEMAN_USE_SYSTEM_BLAKE3` (ON/OFF)
  - `QLTOTAPEMAN_ENABLE_ZSTD` (ON/OFF)
  - `QLTOTAPEMAN_WITH_WIN` (ON/OFF) — 构建 Windows 特定实现
  - `QLTOTAPEMAN_WITH_LINUX` (ON/OFF)
  - `QLTOTAPEMAN_WITH_MAC` (ON/OFF)
- 编译特性：
  - C++20（constexpr/chrono/bit/atomic）
  - `-fPIC` on *nix；MSVC 多线程运行时 `/MT` or `/MD` 由用户自定。
  - 安全：`/permissive-` (MSVC)，`-Wall -Wextra -Wpedantic` (GCC/Clang)；对 Windows SPTD 代码屏蔽不必要警告。

## 5. 平台实现要点（仅列接口约定，后续阶段实现）
- **Windows (`TapeDeviceWin`)**：DeviceIoControl + SCSI_PASS_THROUGH(_DIRECT)；路径兼容 `\\.\TAPE*` 与 `\\.\GLOBALROOT\Device\...`；处理 `Sense` 翻译。
- **Linux (`TapeDeviceSg`)**：`/dev/sg*` + `SG_IO`；必要时打开 `/dev/nst*` 进行顺序读写；检查 CAP_SYS_RAWIO/root，权限不足返回友好错误。
- **macOS (`TapeDeviceMac`)**：IOKit SCSI/StorageFamily；若暂不支持，需在编译期与运行时给出明确提示。

## 6. 线程与锁
- 保留 `SCSIOperationLock` 思路：`TapeDevice` 内部串行化 SCSI 请求；上层 `LtfsService` 在长任务中使用工作线程（QtConcurrent/QThread）。
- 数据管线与 UI 分离：UI 线程仅处理信号，I/O 与计算在后台线程。

## 7. 错误与日志
- 统一返回 `(bool, err string, sense data)`；提供 `sense_to_string` 工具。
- 关键路径记录：装载/退带、读写块、索引写入、容量刷新、eject。

## 8. 占位文件（建议创建，后续阶段填充）
- `src/app/main.cpp`
- `src/io/tape_device.h` (+ `*_win.cpp`, `*_sg.cpp`, `*_mac.cpp` stub)
- `src/io/tape_enumerator.h`
- `src/core/ltfs_label.h/.cpp`
- `src/core/ltfs_index.h/.cpp`
- `src/core/ltfs_service.h/.cpp`
- `src/pipeline/file_record.h/.cpp`
- `src/pipeline/file_data_provider.h/.cpp`
- `CMakeLists.txt` (顶层) + `src/CMakeLists.txt`

---
以上为阶段 0 交付：接口草案 + 构建选项草案。下一阶段可据此落地目录与桩代码。
