# QLTOTapeMan 项目重构规划文档

## 概述

本文档详细规划了将 LTFSCopyGUI (VB.NET + Windows API) 项目重构为 QLTOTapeMan (Qt6 + C++) 跨平台项目的完整步骤。

### 项目目标

1. **跨平台支持**：支持 Windows 和 Linux 平台
2. **Qt6 标准化**：采用 Qt6 框架，遵循 Qt 官方编码规范
3. **松耦合架构**：功能库层与应用层分离，便于二次开发
4. **完整功能复刻**：原样复刻 LTFSCopyGUI 的"直接读写"功能

### 技术选型

- **框架**: Qt 6.x (需要 >= 6.0)
- **语言**: C++17/20
- **编译器支持**: 
  - Windows: MSVC, MinGW
  - Linux: GCC, Clang
- **构建系统**: CMake 3.16+

---

## 项目架构设计

### 整体架构

```
QLTOTapeMan/
├── CMakeLists.txt                    # 顶层 CMake 配置
├── cmake/                            # CMake 辅助模块
│   └── QLTOTapeManConfig.cmake.in
├── src/
│   ├── libqltfs/                     # LTFS 核心功能库（松耦合）
│   │   ├── CMakeLists.txt
│   │   ├── core/                     # 核心数据结构
│   │   │   ├── LtfsIndex.h/cpp       # LTFS 索引结构
│   │   │   ├── LtfsLabel.h/cpp       # LTFS 标签结构
│   │   │   └── LtfsTypes.h           # 类型定义
│   │   ├── device/                   # 设备管理
│   │   │   ├── TapeDevice.h/cpp      # 磁带设备抽象
│   │   │   ├── ScsiCommand.h/cpp     # SCSI 命令封装
│   │   │   ├── DeviceEnumerator.h/cpp# 设备枚举
│   │   │   └── platform/             # 平台特定实现
│   │   │       ├── WinScsi.h/cpp     # Windows SCSI 实现
│   │   │       └── LinuxScsi.h/cpp   # Linux SCSI 实现
│   │   ├── io/                       # IO 管理
│   │   │   ├── TapeIO.h/cpp          # 磁带 IO 操作
│   │   │   ├── BlockManager.h/cpp    # 块管理
│   │   │   └── HashCalculator.h/cpp  # 哈希计算
│   │   ├── xml/                      # XML 解析
│   │   │   ├── IndexParser.h/cpp     # 索引解析
│   │   │   └── IndexWriter.h/cpp     # 索引写入
│   │   └── util/                     # 工具类
│   │       ├── SizeFormatter.h/cpp   # 大小格式化
│   │       └── Logger.h/cpp          # 日志管理
│   │
│   └── app/                          # GUI 应用层
│       ├── CMakeLists.txt
│       ├── main.cpp
│       ├── MainWindow.h/cpp/ui       # 主窗口
│       ├── widgets/                  # 自定义控件
│       │   ├── FileBrowserDialog.h/cpp/ui  # 文件浏览器对话框
│       │   ├── TreeViewEx.h/cpp      # 增强版 TreeView
│       │   └── StatusIndicator.h/cpp # 状态指示器
│       ├── dialogs/                  # 对话框
│       │   ├── LtfsWriterWindow.h/cpp/ui   # LTFS 写入器窗口
│       │   ├── HashTaskWindow.h/cpp/ui     # 哈希任务窗口
│       │   └── SettingsDialog.h/cpp/ui     # 设置对话框
│       ├── models/                   # 数据模型
│       │   ├── FileTreeModel.h/cpp   # 文件树模型
│       │   └── TapeInfoModel.h/cpp   # 磁带信息模型
│       └── resources/                # 资源文件
│           ├── icons/
│           ├── translations/
│           └── app.qrc
├── tests/                            # 单元测试
├── docs/                             # 文档
└── examples/                         # 示例代码
```

### 模块依赖关系

```
┌─────────────────────────────────────────────────────────┐
│                      QLTOTapeMan App                     │
│  (MainWindow, FileBrowserDialog, LtfsWriterWindow...)   │
└─────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────┐
│                      libqltfs                            │
│  ┌─────────────────┐ ┌─────────────┐ ┌───────────────┐  │
│  │   core/         │ │   device/   │ │     io/       │  │
│  │ LtfsIndex       │ │ TapeDevice  │ │   TapeIO      │  │
│  │ LtfsLabel       │ │ ScsiCommand │ │ BlockManager  │  │
│  └─────────────────┘ └─────────────┘ └───────────────┘  │
│  ┌─────────────────┐ ┌─────────────────────────────────┐ │
│  │   xml/          │ │           util/                 │ │
│  │ IndexParser     │ │ SizeFormatter, Logger           │ │
│  │ IndexWriter     │ │                                 │ │
│  └─────────────────┘ └─────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────┐
│              Platform Specific Layer                     │
│   ┌─────────────────────┐  ┌─────────────────────────┐  │
│   │   WinScsi (Windows) │  │   LinuxScsi (Linux)     │  │
│   │   SetupAPI          │  │   sg (SCSI Generic)     │  │
│   └─────────────────────┘  └─────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

---

## 详细实现步骤

### 阶段一：项目基础架构搭建

#### 步骤 1.1：创建 CMake 项目结构

**目标**：建立支持多平台、多编译器的 CMake 构建系统

**参考文件**：无（新创建）

**要点**：
- 顶层 CMakeLists.txt 定义项目名称、版本、C++ 标准
- 设置 Qt6 查找和链接
- 定义编译选项以支持 MSVC、MinGW、GCC、Clang
- 配置安装目标

**CMake 配置示例**：
```cmake
cmake_minimum_required(VERSION 3.16)
project(QLTOTapeMan VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Xml)
```

#### 步骤 1.2：创建 libqltfs 核心库框架

**目标**：建立松耦合的 LTFS 功能库基础结构

**参考文件**：
- `ref_LTFSCopyGUI/LTFSCopyGUI/Schema.vb` (LTFS 数据结构定义)
- `ref_ltfs/src/libltfs/ltfs.h` (官方 LTFS 结构参考)

---

### 阶段二：核心数据结构实现

#### 步骤 2.1：实现 LtfsTypes.h - 基础类型定义

**目标**：定义 LTFS 相关的基础类型和枚举

**参考文件**：
- `ref_LTFSCopyGUI/LTFSCopyGUI/Schema.vb` 第 1-50 行
  - `PartitionLabel` 枚举 (第 16-19 行)
  - `LocationDef` 类 (第 20-25 行)
  - `volumelockstateValue` 枚举 (第 42-46 行)

**实现要点**：
```cpp
namespace qltfs {

enum class PartitionLabel : uint8_t {
    IndexPartition = 0,  // 'a' in LTFS
    DataPartition = 1    // 'b' in LTFS
};

enum class VolumeLockState {
    Unlocked,
    Locked,
    PermLocked
};

struct Location {
    PartitionLabel partition = PartitionLabel::IndexPartition;
    uint64_t startBlock = 0;
};

} // namespace qltfs
```

#### 步骤 2.2：实现 LtfsIndex 类 - LTFS 索引结构

**目标**：实现完整的 LTFS 索引数据结构

**参考文件**：
- `ref_LTFSCopyGUI/LTFSCopyGUI/Schema.vb` 第 1-500 行
  - `ltfsindex` 类 (整个文件)
  - `file` 类 (第 48-270 行)
  - `directory` 类 (第 271-410 行)
  - `contentsDef` 类 (第 411-420 行)
  - `extent` 类 (第 199-230 行)
  - `xattr` 类 (第 120-175 行)

**关键类设计**：

```cpp
// LtfsIndex.h
namespace qltfs {

class LtfsExtent {
    Q_GADGET
public:
    qint64 fileOffset = 0;
    PartitionLabel partition = PartitionLabel::DataPartition;
    qint64 startBlock = 0;
    qint64 byteOffset = 0;
    qint64 byteCount = 0;
};

class LtfsExtendedAttribute {
    Q_GADGET
public:
    QString key;
    QString value;
    
    // 预定义的哈希类型键
    static const QString HashKeyCRC32;
    static const QString HashKeyMD5;
    static const QString HashKeySHA1;
    static const QString HashKeySHA256;
    static const QString HashKeySHA512;
    static const QString HashKeyBLAKE3;
};

class LtfsFile {
    Q_GADGET
public:
    QString name;
    qint64 length = 0;
    bool readOnly = false;
    bool openForWrite = true;
    QString creationTime;
    QString changeTime;
    QString modifyTime;
    QString accessTime;
    QString backupTime;
    qint64 fileUid = 0;
    QString symlink;
    QList<LtfsExtendedAttribute> extendedAttributes;
    QList<LtfsExtent> extentInfo;
    
    // 内部使用
    QString fullPath;
    bool selected = true;
    qint64 writtenBytes = 0;
    
    QString getXAttr(const QString &key) const;
    void setXAttr(const QString &key, const QString &value);
};

class LtfsDirectory {
    Q_GADGET
public:
    QString name;
    bool readOnly = false;
    QString creationTime;
    QString changeTime;
    QString modifyTime;
    QString accessTime;
    QString backupTime;
    qint64 fileUid = 0;
    
    QList<LtfsFile> files;
    QList<LtfsDirectory> directories;
    QList<LtfsFile> unwrittenFiles;
    
    // 内部使用
    QString fullPath;
    bool selected = true;
    
    qint64 totalFiles() const;
    qint64 totalDirectories() const;
    void refreshCount();
};

class LtfsIndex {
    Q_GADGET
public:
    QString creator;
    QUuid volumeUuid;
    quint64 generationNumber = 0;
    QString updateTime;
    Location location;
    Location previousGenerationLocation;
    bool allowPolicyUpdate = false;
    VolumeLockState volumeLockState = VolumeLockState::Unlocked;
    qint64 highestFileUid = 0;
    
    QList<LtfsDirectory> rootDirectories;
    QList<LtfsFile> rootFiles;
    
    // 序列化
    static LtfsIndex fromXml(const QString &xml);
    QString toXml() const;
    LtfsIndex clone() const;
};

} // namespace qltfs
```

#### 步骤 2.3：实现 LtfsLabel 类 - LTFS 标签结构

**目标**：实现 LTFS 卷标签数据结构

**参考文件**：
- `ref_LTFSCopyGUI/LTFSCopyGUI/LTFSWriter.vb` 第 24-30 行 (plabel 属性)
- `ref_ltfs/src/libltfs/label.h` (官方标签结构)

**实现要点**：
```cpp
namespace qltfs {

class LtfsLabel {
    Q_GADGET
public:
    quint32 blockSize = 524288;  // 默认 512KB
    QString volumeName;
    QUuid volumeUuid;
    QString formatTime;
    
    struct PartitionInfo {
        PartitionLabel index = PartitionLabel::IndexPartition;
        PartitionLabel data = PartitionLabel::DataPartition;
    } partitions;
};

} // namespace qltfs
```

---

### 阶段三：设备管理层实现

#### 步骤 3.1：实现 ScsiCommand 类 - SCSI 命令封装

**目标**：封装跨平台的 SCSI 命令执行

**参考文件**：
- `ref_LTFSCopyGUI/LTFSCopyGUI/TapeUtils.vb` 第 346-450 行 (SCSI IOCtl 实现)
- `ref_LTFSCopyGUI/LtfsCommand/tape.c` 第 1-400 行 (ScsiIoControl 函数)
- `ref_ltfs/src/libltfs/tape.h` (官方 SCSI 操作定义)

**Windows 实现参考**：
- `TapeUtils.vb` 中的 `IOCtl` 类 (第 363-450 行)
- `SCSI_PASS_THROUGH_DIRECT` 结构定义 (第 371-395 行)
- `DeviceIoControl` 调用 (第 295-310 行)

**Linux 实现参考**：
- `ref_ltfs/src/tape_drivers/linux/sg/sg_tape.c`

**实现要点**：
```cpp
namespace qltfs {

class ScsiCommand {
public:
    // 数据方向
    enum class DataDirection {
        None = 0,
        In = 1,
        Out = 0
    };
    
    // SCSI 命令执行结果
    struct Result {
        bool success = false;
        QByteArray senseData;
        QByteArray responseData;
        QString errorMessage;
    };
    
    static Result execute(
        void *deviceHandle,
        const QByteArray &cdb,
        QByteArray &dataBuffer,
        DataDirection direction,
        quint32 timeout = 60000
    );
    
    static QString parseSenseData(const QByteArray &sense);
};

} // namespace qltfs
```

#### 步骤 3.2：实现 DeviceEnumerator 类 - 设备枚举

**目标**：实现跨平台的磁带设备枚举

**参考文件**：
- `ref_LTFSCopyGUI/LTFSCopyGUI/TapeUtils.vb` 第 29-180 行 (SetupAPIWheels 类)
- `ref_LTFSCopyGUI/LtfsCommand/tape.c` 第 140-290 行 (TapeGetDriveList 函数)
- `ref_LTFSCopyGUI/LtfsCommand/tape.h` 第 24-36 行 (TAPE_DRIVE 结构)

**Windows 实现参考**：
- SetupAPI 调用: `SetupDiGetClassDevs`, `SetupDiEnumDeviceInterfaces` 等
- GUID_DEVINTERFACE_TAPE: `{53f5630b-b6bf-11d0-94f2-00a0c91efb8b}`
- GUID_DEVINTERFACE_MEDIUMCHANGER: `{53f56310-b6bf-11d0-94f2-00a0c91efb8b}`

**Linux 实现**：
- 扫描 `/dev/st*`, `/dev/nst*` 设备
- 使用 SCSI Inquiry 命令获取设备信息

**实现要点**：
```cpp
namespace qltfs {

struct TapeDriveInfo {
    QString vendorId;
    QString productId;
    QString serialNumber;
    QString devicePath;    // Windows: \\.\Tape0, Linux: /dev/st0
    int deviceIndex = -1;
};

class DeviceEnumerator {
public:
    static QList<TapeDriveInfo> enumerateTapeDrives();
    static QList<TapeDriveInfo> enumerateMediumChangers();
    
private:
#ifdef Q_OS_WIN
    static QList<TapeDriveInfo> enumerateWin(const QUuid &interfaceGuid);
#else
    static QList<TapeDriveInfo> enumerateLinux();
#endif
};

} // namespace qltfs
```

#### 步骤 3.3：实现 TapeDevice 类 - 磁带设备操作

**目标**：实现完整的磁带设备操作接口

**参考文件**：
- `ref_LTFSCopyGUI/LTFSCopyGUI/TapeUtils.vb` 第 500-1500 行
  - `OpenTapeDrive` / `CloseTapeDrive` (第 553-650 行)
  - `Inquiry` (第 730-780 行)
  - `ReadPosition` (参考 PositionData 类)
  - `Locate` (第 1095-1200 行)
  - `ReadBlock` / `WriteBlock` 操作
  - `ReadFileMark` / `WriteFileMark` 操作
  - `TestUnitReady` (第 705-720 行)
  - `Load` / `Eject` 操作

**关键操作映射**：
| VB 原函数 | Qt/C++ 新函数 | SCSI 命令 |
|-----------|---------------|-----------|
| `Inquiry` | `inquiry()` | 0x12 |
| `TestUnitReady` | `testUnitReady()` | 0x00 |
| `ReadPosition` | `readPosition()` | 0x34 |
| `Locate` | `locate()` | 0x92/0x2B |
| `ReadBlock` | `readBlock()` | 0x08 |
| `WriteBlock` | `writeBlock()` | 0x0A |
| `ReadFileMark` | `isAtFileMark()` | 读取后检测 |
| `WriteFileMark` | `writeFileMark()` | 0x10 |
| `Space6` | `space()` | 0x11 |
| `Flush` | `flush()` | 0x10 (WriteFileMarks) |
| `Load` | `load()` | 0x1B |
| `Eject` | `eject()` | 0x1B |

**实现要点**：
```cpp
namespace qltfs {

class TapeDevice : public QObject {
    Q_OBJECT
public:
    struct Position {
        uint64_t blockNumber = 0;
        uint8_t partitionNumber = 0;
        bool isAtBOT = false;
        bool isAtEOD = false;
    };
    
    struct DriveInfo {
        QString vendorId;
        QString productId;
        QString serialNumber;
    };
    
    // 设备管理
    bool open(const QString &devicePath);
    void close();
    bool isOpen() const;
    
    // 信息查询
    DriveInfo inquiry();
    Position readPosition();
    bool testUnitReady(QByteArray *sense = nullptr);
    QString readBarcode();
    quint64 readRemainingCapacity(uint8_t partition = 0);
    
    // 定位操作
    bool locate(uint64_t blockAddress, uint8_t partition);
    bool locateToFileMark(uint64_t count, uint8_t partition);
    bool locateToEOD(uint8_t partition);
    bool space(int count, SpaceType type = SpaceType::Blocks);
    bool rewind();
    
    // 读写操作
    QByteArray readBlock(QByteArray *sense = nullptr, quint32 maxSize = 0x80000);
    bool writeBlock(const QByteArray &data);
    bool writeFileMark(int count = 1);
    bool flush();
    
    // 介质操作
    bool load();
    bool eject();
    bool preventRemoval();
    bool allowRemoval();
    
signals:
    void operationStarted();
    void operationFinished();
    void errorOccurred(const QString &message);
    
private:
    void *m_handle = nullptr;
    QString m_devicePath;
    mutable QMutex m_mutex;
};

} // namespace qltfs
```

---

### 阶段四：IO 管理层实现

#### 步骤 4.1：实现 TapeIO 类 - 磁带 IO 管理

**目标**：实现高级别的磁带读写操作

**参考文件**：
- `ref_LTFSCopyGUI/LTFSCopyGUI/TapeUtils.vb` 第 1000-1100 行 (ReadToFileMark 等)
- `ref_LTFSCopyGUI/LTFSCopyGUI/LTFSWriter.vb` 第 1000-2000 行 (文件写入逻辑)

**实现要点**：
```cpp
namespace qltfs {

class TapeIO : public QObject {
    Q_OBJECT
public:
    explicit TapeIO(TapeDevice *device, QObject *parent = nullptr);
    
    // 读取到文件标记
    QByteArray readToFileMark(quint32 blockSizeLimit = 0x80000);
    bool readToFileMark(const QString &outputFileName, quint32 blockSizeLimit = 0x80000);
    
    // 写入文件
    bool writeFile(const QString &sourcePath, LtfsFile *fileEntry);
    bool writeFileFromBuffer(const QByteArray &data, LtfsFile *fileEntry);
    
    // 读取文件
    bool readFile(const LtfsFile &fileEntry, const QString &destPath);
    QByteArray readFileToBuffer(const LtfsFile &fileEntry);
    
signals:
    void progressChanged(qint64 bytesProcessed, qint64 totalBytes);
    void fileStarted(const QString &fileName);
    void fileFinished(const QString &fileName);
    void error(const QString &message);
    
private:
    TapeDevice *m_device;
    quint32 m_blockSize = 524288;
};

} // namespace qltfs
```

#### 步骤 4.2：实现 HashCalculator 类 - 哈希计算

**目标**：实现多种哈希算法的计算

**参考文件**：
- `ref_LTFSCopyGUI/LTFSCopyGUI/IOManager.vb` 第 125-200 行 (SHA1 等哈希函数)
- `ref_LTFSCopyGUI/LTFSCopyGUI/Schema.vb` 第 120-175 行 (xattr.HashType)

**支持的哈希类型**：
- CRC32 (4 bytes)
- MD5 (16 bytes)
- SHA1 (20 bytes)
- SHA256 (32 bytes)
- SHA512 (64 bytes)
- BLAKE3 (32 bytes)
- xxHash3 (8 bytes)
- xxHash128 (16 bytes)

**实现要点**：
```cpp
namespace qltfs {

class HashCalculator : public QObject {
    Q_OBJECT
public:
    enum class Algorithm {
        CRC32,
        MD5,
        SHA1,
        SHA256,
        SHA512,
        BLAKE3,
        XxHash3,
        XxHash128
    };
    
    static QString calculate(const QString &filePath, Algorithm algorithm);
    static QString calculate(const QByteArray &data, Algorithm algorithm);
    static QString calculateAsync(const QString &filePath, Algorithm algorithm);
    
signals:
    void progressChanged(qint64 bytesProcessed, qint64 totalBytes);
    void finished(const QString &hash);
};

} // namespace qltfs
```

---

### 阶段五：XML 解析层实现

#### 步骤 5.1：实现 IndexParser 类 - 索引解析

**目标**：实现 LTFS 索引 XML 的解析

**参考文件**：
- `ref_LTFSCopyGUI/LTFSCopyGUI/Schema.vb` 第 770-890 行 (FromXML, FromSchemaText)
- `ref_ltfs/src/libltfs/xml_reader_libltfs.c` (官方 XML 解析实现)

**XML 结构示例**：
```xml
<ltfsindex version="2.4.0">
    <creator>QLTOTapeMan 1.0.0</creator>
    <volumeuuid>...</volumeuuid>
    <generationnumber>1</generationnumber>
    <updatetime>2024-01-01T00:00:00.000000000Z</updatetime>
    <location>
        <partition>a</partition>
        <startblock>5</startblock>
    </location>
    <directory>
        <name>/</name>
        <creationtime>...</creationtime>
        <contents>
            <file>...</file>
            <directory>...</directory>
        </contents>
    </directory>
</ltfsindex>
```

**实现要点**：
```cpp
namespace qltfs {

class IndexParser {
public:
    static LtfsIndex parse(const QString &xml);
    static LtfsIndex parseFile(const QString &filePath);
    
    struct ParseError {
        int line = 0;
        int column = 0;
        QString message;
    };
    
    static ParseError lastError();
    
private:
    static LtfsDirectory parseDirectory(QXmlStreamReader &reader);
    static LtfsFile parseFile(QXmlStreamReader &reader);
    static LtfsExtent parseExtent(QXmlStreamReader &reader);
};

} // namespace qltfs
```

#### 步骤 5.2：实现 IndexWriter 类 - 索引写入

**目标**：实现 LTFS 索引 XML 的生成

**参考文件**：
- `ref_LTFSCopyGUI/LTFSCopyGUI/Schema.vb` 第 230-270 行 (GetSerializedText)
- `ref_ltfs/src/libltfs/xml_writer_libltfs.c` (官方 XML 写入实现)

**实现要点**：
```cpp
namespace qltfs {

class IndexWriter {
public:
    static QString write(const LtfsIndex &index, bool prettyPrint = false);
    static bool writeFile(const LtfsIndex &index, const QString &filePath);
    
private:
    static void writeDirectory(QXmlStreamWriter &writer, const LtfsDirectory &dir);
    static void writeFile(QXmlStreamWriter &writer, const LtfsFile &file);
    static void writeExtent(QXmlStreamWriter &writer, const LtfsExtent &extent);
};

} // namespace qltfs
```

---

### 阶段六：GUI 应用层实现

#### 步骤 6.1：实现 MainWindow - 主窗口

**目标**：实现应用程序主窗口

**参考文件**：
- `ref_LTFSCopyGUI/LTFSCopyGUI/Form1.vb` (主窗口逻辑)
- `ref_LTFSCopyGUI/LTFSCopyGUI/Form1.Designer.vb` (主窗口设计)

**UI 元素**：
- 磁带机选择下拉框
- 索引文件加载按钮
- 文件列表显示
- 状态栏

**实现要点**：
```cpp
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    
private slots:
    void onRefreshDevices();
    void onLoadIndex();
    void onOpenFileBrowser();
    void onOpenLtfsWriter();
    
private:
    void setupUi();
    void setupConnections();
    void refreshDeviceList();
    
    Ui::MainWindow *ui;
    qltfs::LtfsIndex m_currentIndex;
    QList<qltfs::TapeDriveInfo> m_tapeDevices;
};
```

#### 步骤 6.2：实现 FileBrowserDialog - 文件浏览器对话框（核心功能）

**目标**：原样复刻 LTFSCopyGUI 的文件浏览器对话框

**参考文件**：
- `ref_LTFSCopyGUI/LTFSCopyGUI/FileBrowser.vb` (完整逻辑)
- `ref_LTFSCopyGUI/LTFSCopyGUI/FileBrowser.Designer.vb` (UI 设计)
- `ref_LTFSCopyGUI/LTFSCopyGUI/TreeViewEx.vb` (增强版 TreeView)

**关键功能**：
1. **三态复选框 TreeView** (TreeViewEx)
   - 参考: `TreeViewEx.vb`
   - 支持 Checked, Unchecked, Indeterminate 三种状态
   - 父节点状态根据子节点自动更新

2. **递归选择** (RecursivelySetNodeCheckStatus)
   - 参考: `FileBrowser.vb` 第 86-93 行
   - 选中父节点时自动选中所有子节点

3. **选择状态刷新** (RefreshChackState)
   - 参考: `FileBrowser.vb` 第 98-130 行
   - 根据子节点状态更新父节点显示状态

4. **右键菜单选择功能**：
   - 全选 (第 166-170 行)
   - 按大小筛选 (第 172-205 行)
   - 按文件名正则匹配 (第 207-245 行)

**UI 布局**：
```
┌────────────────────────────────────────────────────┐
│ FileBrowser                                    [X] │
├────────────────────────────────────────────────────┤
│ ┌────────────────────────────────────────────────┐ │
│ │ TreeView (带复选框)                            │ │
│ │  ☑ Root                                        │ │
│ │   ☑ Folder1                                    │ │
│ │     ☑ File1.txt                                │ │
│ │     ☐ File2.txt                                │ │
│ │   ☐ Folder2                                    │ │
│ │     ☐ File3.txt                                │ │
│ └────────────────────────────────────────────────┘ │
│ ☑ 复制信息到剪贴板                                  │
├────────────────────────────────────────────────────┤
│              [确定]          [取消]                 │
└────────────────────────────────────────────────────┘
```

**实现要点**：
```cpp
class FileBrowserDialog : public QDialog {
    Q_OBJECT
public:
    explicit FileBrowserDialog(QWidget *parent = nullptr);
    
    void setIndex(const qltfs::LtfsIndex &index);
    qltfs::LtfsIndex getModifiedIndex() const;
    
    static DialogCode showDialog(qltfs::LtfsIndex &index, QWidget *parent = nullptr);
    
private slots:
    void onItemChanged(QTreeWidgetItem *item, int column);
    void onItemSelectionChanged();
    void onSelectAll();
    void onSelectBySize();
    void onSelectByRegex();
    
private:
    void buildTree();
    void addItem(QTreeWidgetItem *parent, const qltfs::LtfsDirectory &dir);
    void addItem(QTreeWidgetItem *parent, const qltfs::LtfsFile &file);
    void recursivelySetCheckState(QTreeWidgetItem *item, Qt::CheckState state);
    void refreshCheckState(QTreeWidgetItem *item);
    void updateIndexSelection();
    
    Ui::FileBrowserDialog *ui;
    qltfs::LtfsIndex m_index;
    bool m_eventLock = false;
};
```

#### 步骤 6.3：实现 TreeViewEx - 增强版 TreeView

**目标**：实现支持三态复选框的 TreeWidget

**参考文件**：
- `ref_LTFSCopyGUI/LTFSCopyGUI/TreeViewEx.vb` (完整实现)

**关键功能**：
- 三态复选框支持
- 自动绘制 Indeterminate 状态
- 节点状态改变信号

**实现要点**：
```cpp
class TreeViewEx : public QTreeWidget {
    Q_OBJECT
public:
    explicit TreeViewEx(QWidget *parent = nullptr);
    
    void setNodeCheckState(QTreeWidgetItem *item, Qt::CheckState state);
    Qt::CheckState nodeCheckState(QTreeWidgetItem *item) const;
    
signals:
    void checkStateChanged(QTreeWidgetItem *item, Qt::CheckState state);
    
protected:
    void drawRow(QPainter *painter, const QStyleOptionViewItem &options, 
                 const QModelIndex &index) const override;
    
private:
    QMap<QTreeWidgetItem*, Qt::CheckState> m_checkStates;
};
```

#### 步骤 6.4：实现 LtfsWriterWindow - LTFS 写入器窗口

**目标**：实现完整的 LTFS 写入器窗口（"直接读写"功能）

**参考文件**：
- `ref_LTFSCopyGUI/LTFSCopyGUI/LTFSWriter.vb` (完整逻辑，约 9388 行)
- `ref_LTFSCopyGUI/LTFSCopyGUI/LTFSWriter.Designer.vb` (UI 设计)

**核心功能**：

1. **磁带状态显示**
   - 参考: `LTFSWriter.vb` 第 350-500 行 (Timer2_Tick, SetStatusLight)
   - 状态指示灯: NotReady, Idle, Busy, Success, Error
   - 速度图表显示

2. **文件列表管理**
   - ListView 显示当前目录内容
   - 支持拖放添加文件
   - 上下文菜单操作

3. **写入操作**
   - 参考: `LTFSWriter.vb` 写入相关函数
   - 进度显示
   - 速度限制
   - 暂停/继续
   - 索引更新间隔

4. **设置选项**
   - 覆盖已有文件
   - 跳过符号链接
   - 计算校验
   - 自动刷新

**UI 布局**：
```
┌─────────────────────────────────────────────────────────────────┐
│ LTFSWriter - [磁带条码]                                     [X] │
├─────────────────────────────────────────────────────────────────┤
│ 文件(F)  编辑(E)  视图(V)  工具(T)  设置(S)  帮助(H)           │
├─────────────────────────────────────────────────────────────────┤
│ ┌─ 导航树 ─────┐ ┌─ 文件列表 ──────────────────────────────────┐ │
│ │ / (Root)      │ │ 名称          大小       修改时间           │ │
│ │  ├─ Folder1   │ │ ├─ File1.txt  1.2 MB    2024-01-01 12:00   │ │
│ │  └─ Folder2   │ │ └─ File2.txt  3.4 MB    2024-01-01 12:01   │ │
│ └───────────────┘ └─────────────────────────────────────────────┘ │
│ ┌─ 速度图表 ───────────────────────────────────────────────────┐ │
│ │ [速度曲线图]                                                  │ │
│ └───────────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────┤
│ ● ● ● ○ ○ | 速度: 150 MB/s | 已写: 10.5 GB | 剩余: 89.5 GB    │
└─────────────────────────────────────────────────────────────────┘
```

**实现要点**：
```cpp
class LtfsWriterWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit LtfsWriterWindow(QWidget *parent = nullptr);
    
    void setTapeDevice(const QString &devicePath);
    void setIndex(const qltfs::LtfsIndex &index);
    
signals:
    void ltfsLoaded();
    void writeFinished();
    void tapeEjected();
    
private slots:
    void onTimerTick();
    void onOpenFileBrowser();
    void onStartWrite();
    void onPauseWrite();
    void onStopWrite();
    void onEjectTape();
    void onUpdateIndex();
    
private:
    void setupUi();
    void setupMenus();
    void setupStatusBar();
    void updateSpeedChart();
    void updateStatusIndicators();
    
    Ui::LtfsWriterWindow *ui;
    qltfs::TapeDevice *m_device;
    qltfs::TapeIO *m_tapeIO;
    qltfs::LtfsIndex m_index;
    qltfs::LtfsLabel m_label;
    
    // 状态变量
    bool m_isWriting = false;
    bool m_isPaused = false;
    qint64 m_totalBytesProcessed = 0;
    qint64 m_totalFilesProcessed = 0;
    QList<double> m_speedHistory;
    QTimer *m_statusTimer;
};
```

#### 步骤 6.5：实现 FileTreeModel - 文件树模型

**目标**：实现 Qt Model/View 架构的文件树模型

**参考文件**：
- `ref_LTFSCopyGUI/LTFSCopyGUI/FileDataProvider.vb` (数据提供器)

**实现要点**：
```cpp
class FileTreeModel : public QAbstractItemModel {
    Q_OBJECT
public:
    explicit FileTreeModel(QObject *parent = nullptr);
    
    void setIndex(const qltfs::LtfsIndex &index);
    qltfs::LtfsIndex getIndex() const;
    
    // QAbstractItemModel 接口
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    
private:
    struct TreeNode {
        enum class Type { Root, Directory, File };
        Type type;
        QString name;
        qint64 size = 0;
        Qt::CheckState checkState = Qt::Checked;
        TreeNode *parent = nullptr;
        QList<TreeNode*> children;
        void *dataPtr = nullptr;  // 指向 LtfsDirectory 或 LtfsFile
    };
    
    TreeNode *m_rootNode = nullptr;
    qltfs::LtfsIndex m_index;
};
```

#### 步骤 6.6：实现 HashTaskWindow - 哈希任务窗口

**目标**：实现文件哈希计算任务窗口

**参考文件**：
- `ref_LTFSCopyGUI/LTFSCopyGUI/HashTaskWindow.vb`
- `ref_LTFSCopyGUI/LTFSCopyGUI/HashTaskWindow.Designer.vb`

**实现要点**：
```cpp
class HashTaskWindow : public QDialog {
    Q_OBJECT
public:
    explicit HashTaskWindow(QWidget *parent = nullptr);
    
    void setIndex(const qltfs::LtfsIndex &index);
    void setBaseDirectory(const QString &baseDir);
    void setTargetDirectory(const QString &targetDir);
    
private slots:
    void onStartHash();
    void onStopHash();
    void onHashProgress(qint64 bytesProcessed, qint64 totalBytes);
    void onHashFinished(const QString &filePath, const QString &hash);
    
private:
    Ui::HashTaskWindow *ui;
    qltfs::LtfsIndex m_index;
    QString m_baseDirectory;
    QString m_targetDirectory;
    QThread *m_hashThread = nullptr;
};
```

---

### 阶段七：工具类和辅助功能实现

#### 步骤 7.1：实现 SizeFormatter - 大小格式化

**目标**：实现文件大小的人性化格式化显示

**参考文件**：
- `ref_LTFSCopyGUI/LTFSCopyGUI/IOManager.vb` 第 38-65 行 (FormatSize 函数)

**实现要点**：
```cpp
namespace qltfs {

class SizeFormatter {
public:
    enum class Unit {
        Binary,   // KiB, MiB, GiB (1024)
        Decimal   // KB, MB, GB (1000)
    };
    
    static QString format(qint64 bytes, Unit unit = Unit::Binary, bool extended = false);
    static QString formatSpeed(qint64 bytesPerSecond, Unit unit = Unit::Binary);
};

} // namespace qltfs
```

#### 步骤 7.2：实现 Logger - 日志管理

**目标**：实现统一的日志管理

**参考文件**：
- `ref_LTFSCopyGUI/LTFSCopyGUI/LTFSWriter.vb` 第 460-510 行 (PrintMsg, logBuffer)

**实现要点**：
```cpp
namespace qltfs {

class Logger : public QObject {
    Q_OBJECT
public:
    enum class Level {
        Debug,
        Info,
        Warning,
        Error
    };
    
    static Logger* instance();
    
    void setLogFile(const QString &filePath);
    void setLogLevel(Level level);
    void log(Level level, const QString &message, const QString &context = QString());
    void flush();
    
signals:
    void messageLogged(Level level, const QString &message);
    
private:
    Logger();
    QString m_logFilePath;
    Level m_logLevel = Level::Info;
    QMutex m_mutex;
    QStringList m_buffer;
};

#define QLTFS_LOG_DEBUG(msg) qltfs::Logger::instance()->log(qltfs::Logger::Level::Debug, msg)
#define QLTFS_LOG_INFO(msg) qltfs::Logger::instance()->log(qltfs::Logger::Level::Info, msg)
#define QLTFS_LOG_WARN(msg) qltfs::Logger::instance()->log(qltfs::Logger::Level::Warning, msg)
#define QLTFS_LOG_ERROR(msg) qltfs::Logger::instance()->log(qltfs::Logger::Level::Error, msg)

} // namespace qltfs
```

---

### 阶段八：平台特定实现

#### 步骤 8.1：Windows 平台 SCSI 实现

**目标**：实现 Windows 平台的 SCSI 通信

**参考文件**：
- `ref_LTFSCopyGUI/LTFSCopyGUI/TapeUtils.vb` 第 285-450 行
- `ref_LTFSCopyGUI/LtfsCommand/tape.c` 第 640-794 行 (ScsiIoControl)

**关键 API**：
- `CreateFile` - 打开设备
- `DeviceIoControl` - 发送 SCSI 命令
- `IOCTL_SCSI_PASS_THROUGH_DIRECT` - SCSI 直通

**实现文件**：`src/libqltfs/device/platform/WinScsi.cpp`

#### 步骤 8.2：Linux 平台 SCSI 实现

**目标**：实现 Linux 平台的 SCSI 通信

**参考文件**：
- `ref_ltfs/src/tape_drivers/linux/sg/sg_tape.c`

**关键 API**：
- `open()` - 打开设备 (/dev/st*, /dev/nst*)
- `ioctl(SG_IO)` - 发送 SCSI 命令
- `sg_io_hdr_t` 结构

**实现文件**：`src/libqltfs/device/platform/LinuxScsi.cpp`

---

### 阶段九：资源和国际化

#### 步骤 9.1：资源文件整理

**目标**：整理图标、翻译等资源文件

**参考文件**：
- `ref_LTFSCopyGUI/LTFSCopyGUI/res/` (图标资源)
- `ref_LTFSCopyGUI/LTFSCopyGUI/*.resx` (多语言资源)

**资源结构**：
```
src/app/resources/
├── icons/
│   ├── app.ico
│   ├── folder.png
│   ├── file.png
│   ├── tape.png
│   └── ...
├── translations/
│   ├── qltotapeman_zh_CN.ts
│   ├── qltotapeman_zh_TW.ts
│   └── qltotapeman_en.ts
└── app.qrc
```

#### 步骤 9.2：多语言支持

**目标**：实现多语言界面支持

**参考文件**：
- `ref_LTFSCopyGUI/LTFSCopyGUI/FileBrowser.zh.resx`
- `ref_LTFSCopyGUI/LTFSCopyGUI/FileBrowser.zh-Hant.resx`
- `ref_LTFSCopyGUI/LTFSCopyGUI/FileBrowser.en.resx`

**实现要点**：
- 使用 Qt Linguist 工具
- tr() 宏包装所有用户可见字符串
- 支持简体中文、繁体中文、英语

---

### 阶段十：测试和文档

#### 步骤 10.1：单元测试

**目标**：为核心功能编写单元测试

**测试范围**：
- LtfsIndex 序列化/反序列化
- XML 解析/生成
- 哈希计算
- 设备枚举（模拟）

#### 步骤 10.2：集成测试

**目标**：验证完整功能流程

**测试场景**：
- 加载 LTFS 索引文件
- 文件浏览器选择操作
- 写入操作流程

#### 步骤 10.3：文档编写

**目标**：编写项目文档

**文档内容**：
- README.md - 项目介绍和快速开始
- BUILDING.md - 编译指南
- API.md - libqltfs API 文档
- CONTRIBUTING.md - 贡献指南

---

## 文件对照表

| 原项目文件 | 新项目文件 | 说明 |
|------------|------------|------|
| Schema.vb | libqltfs/core/LtfsIndex.h/cpp | LTFS 索引结构 |
| TapeUtils.vb | libqltfs/device/TapeDevice.h/cpp | 磁带设备操作 |
| TapeUtils.vb (SCSI) | libqltfs/device/ScsiCommand.h/cpp | SCSI 命令封装 |
| TapeUtils.vb (Enum) | libqltfs/device/DeviceEnumerator.h/cpp | 设备枚举 |
| IOManager.vb | libqltfs/io/TapeIO.h/cpp | IO 管理 |
| IOManager.vb (Hash) | libqltfs/util/HashCalculator.h/cpp | 哈希计算 |
| Form1.vb | app/MainWindow.h/cpp/ui | 主窗口 |
| FileBrowser.vb | app/widgets/FileBrowserDialog.h/cpp/ui | 文件浏览器 |
| TreeViewEx.vb | app/widgets/TreeViewEx.h/cpp | 增强版 TreeView |
| LTFSWriter.vb | app/dialogs/LtfsWriterWindow.h/cpp/ui | LTFS 写入器 |
| HashTaskWindow.vb | app/dialogs/HashTaskWindow.h/cpp/ui | 哈希任务窗口 |
| FileDataProvider.vb | app/models/FileTreeModel.h/cpp | 文件树模型 |

---

## 实现优先级

### 高优先级（核心功能）
1. LtfsIndex, LtfsLabel 数据结构
2. TapeDevice 设备操作
3. FileBrowserDialog 文件浏览器
4. XML 解析/生成

### 中优先级（完整功能）
5. LtfsWriterWindow 写入器窗口
6. 哈希计算功能
7. 设备枚举
8. 日志管理

### 低优先级（增强功能）
9. 多语言支持
10. 完整的错误处理
11. 性能优化
12. 单元测试

---

## 编码规范

### 命名规范

遵循 Qt 官方编码规范：

- **类名**: 大驼峰，如 `LtfsIndex`, `TapeDevice`
- **成员变量**: `m_` 前缀 + 小驼峰，如 `m_devicePath`
- **函数名**: 小驼峰，如 `readPosition()`, `writeBlock()`
- **常量**: 全大写 + 下划线，如 `DEFAULT_BLOCK_SIZE`
- **枚举值**: 大驼峰，如 `PartitionLabel::IndexPartition`
- **信号**: 无前缀，如 `progressChanged()`
- **槽函数**: `on` 前缀，如 `onButtonClicked()`
- **私有槽**: `on` + 控件名 + 信号名，如 `onOkButtonClicked()`

### 文件组织

- 每个类一个头文件 + 一个源文件
- 头文件使用 `#pragma once`
- 使用前向声明减少头文件依赖
- 私有实现使用 Pimpl 模式（可选）

### Qt 特定规范

- 使用 Q_OBJECT 宏的类必须继承 QObject
- 信号槽使用新式语法：`connect(sender, &Sender::signal, receiver, &Receiver::slot)`
- 资源管理使用 QObject 父子关系或智能指针
- 字符串使用 QString，不使用 std::string

---

## 附录

### A. SCSI 命令参考

| 命令 | 操作码 | 说明 |
|------|--------|------|
| TEST UNIT READY | 0x00 | 测试设备就绪 |
| INQUIRY | 0x12 | 查询设备信息 |
| READ (6) | 0x08 | 读取数据块 |
| WRITE (6) | 0x0A | 写入数据块 |
| WRITE FILEMARKS (6) | 0x10 | 写入文件标记 |
| SPACE (6) | 0x11 | 空间移动 |
| MODE SENSE (6) | 0x1A | 模式感知 |
| MODE SELECT (6) | 0x15 | 模式选择 |
| LOAD/UNLOAD | 0x1B | 加载/弹出 |
| LOCATE (10) | 0x2B | 定位（短） |
| READ POSITION | 0x34 | 读取位置 |
| LOG SENSE | 0x4D | 日志感知 |
| LOCATE (16) | 0x92 | 定位（长） |

### B. LTFS 索引 XML Schema 简要

```xml
<!-- 根元素 -->
<ltfsindex version="2.4.0">
    <creator>string</creator>
    <volumeuuid>uuid</volumeuuid>
    <generationnumber>integer</generationnumber>
    <updatetime>datetime</updatetime>
    <location>
        <partition>a|b</partition>
        <startblock>integer</startblock>
    </location>
    <previousgenerationlocation>...</previousgenerationlocation>
    <allowpolicyupdate>boolean</allowpolicyupdate>
    <volumelockstate>unlocked|locked|permlocked</volumelockstate>
    <highestfileuid>integer</highestfileuid>
    <directory>
        <name>string</name>
        <readonly>boolean</readonly>
        <creationtime>datetime</creationtime>
        <changetime>datetime</changetime>
        <modifytime>datetime</modifytime>
        <accesstime>datetime</accesstime>
        <backuptime>datetime</backuptime>
        <fileuid>integer</fileuid>
        <contents>
            <file>
                <name>string</name>
                <length>integer</length>
                <readonly>boolean</readonly>
                <creationtime>datetime</creationtime>
                <!-- ... 其他时间字段 -->
                <fileuid>integer</fileuid>
                <symlink>string</symlink>
                <extendedattributes>
                    <xattr>
                        <key>string</key>
                        <value>string</value>
                    </xattr>
                </extendedattributes>
                <extentinfo>
                    <extent>
                        <fileoffset>integer</fileoffset>
                        <partition>a|b</partition>
                        <startblock>integer</startblock>
                        <byteoffset>integer</byteoffset>
                        <bytecount>integer</bytecount>
                    </extent>
                </extentinfo>
            </file>
            <directory>...</directory>
        </contents>
    </directory>
</ltfsindex>
```

### C. 编译器兼容性注意事项

**Windows MSVC**：
- 使用 `#pragma warning(disable: ...)` 禁用特定警告
- 注意 UNICODE 定义
- SetupAPI 需要链接 `setupapi.lib`

**Windows MinGW**：
- 需要 Windows SDK 头文件
- 可能需要特殊的 SCSI 头文件处理

**Linux GCC/Clang**：
- 需要 root 权限或适当的设备权限访问 /dev/st*
- 需要链接 `-lpthread`

---

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0 | 2026-01-14 | 初始规划文档 |

---

*本文档将随项目进展持续更新*
