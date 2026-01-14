# QLTOTapeMan 重构规划文档

> **项目目标**：采用 Qt6 + C++ 重构 LTFSCopyGUI 的"直接读写"功能，构建一个跨平台、专业易用的 LTO 磁带 LTFS 管理软件。

---

## 目录

1. [项目概述](#1-项目概述)
2. [总体架构设计](#2-总体架构设计)
3. [模块划分与依赖关系](#3-模块划分与依赖关系)
4. [重构步骤详细规划](#4-重构步骤详细规划)
5. [代码映射参考表](#5-代码映射参考表)
6. [UI 设计参考](#6-ui-设计参考)
7. [跨平台兼容性考虑](#7-跨平台兼容性考虑)
8. [测试策略](#8-测试策略)
9. [项目结构规划](#9-项目结构规划)

---

## 1. 项目概述

### 1.1 背景

LTFSCopyGUI 是一个使用 VB.NET 编写的 Windows 专用 LTFS 管理工具，功能强大但缺乏跨平台能力。本项目旨在使用 Qt6 + C++ 完整重构其"直接读写"功能，实现以下目标：

- **跨平台支持**：Windows、Linux、macOS
- **松耦合架构**：功能库与 UI 分离，便于二次开发
- **Qt 标准规范**：遵循 Qt 命名和编码规范
- **多编译器支持**：MSVC、MinGW、GCC、Clang

### 1.2 原项目核心功能分析

原项目 `LTFSCopyGUI` 包含以下核心模块（位于 `ref_LTFSCopyGUI/`）：

| 文件 | 功能描述 | 代码行数 |
|------|----------|----------|
| `TapeUtils.vb` | SCSI 命令发送、磁带机操作核心 | ~9096 行 |
| `LTFSWriter.vb` | 直接读写功能主界面与逻辑 | ~9388 行 |
| `Schema.vb` | LTFS 索引结构定义与序列化 | ~890 行 |
| `IOManager.vb` | 文件 I/O、哈希计算、格式转换 | ~2685 行 |
| `FileBrowser.vb` | 文件选择浏览器对话框 | ~287 行 |
| `LTFSConfigurator.vb` | 磁带配置管理器界面 | ~2408 行 |
| `LtfsCommand/tape.c` | 底层 SCSI 操作 C 代码 | ~794 行 |

### 1.3 本次重构范围

**核心目标**：完整复刻"直接读写"功能，包括：

1. **LTFSWriter 界面**：独立文件管理器窗口
2. **FileBrowser 对话框**：文件选择与预览
3. **磁带机操作**：SCSI 命令、设备管理
4. **LTFS 索引管理**：读写、解析、更新
5. **数据读写**：块级读写操作

---

## 2. 总体架构设计

### 2.1 分层架构

```
┌─────────────────────────────────────────────────────────────┐
│                    QLTOTapeMan Application                   │
│                         (UI Layer)                           │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐   │
│  │ MainWindow   │  │ LTFSWriter   │  │ FileBrowser      │   │
│  │ (主窗口)     │  │ (直接读写)   │  │ (文件浏览器)     │   │
│  └──────────────┘  └──────────────┘  └──────────────────┘   │
│                                                              │
├─────────────────────────────────────────────────────────────┤
│                    QLtfs Library (Core)                      │
│                      (功能库层)                              │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │                  QLtfsIndex                           │   │
│  │         (LTFS 索引结构与序列化)                       │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │                  QLtfsTape                            │   │
│  │         (磁带机操作抽象层)                            │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │                  QLtfsIO                              │   │
│  │         (文件 I/O 与哈希计算)                         │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                              │
├─────────────────────────────────────────────────────────────┤
│               Platform Abstraction Layer                     │
│                   (平台抽象层)                               │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ Windows     │  │ Linux       │  │ macOS               │  │
│  │ SCSI/SPTI   │  │ sg/SCSI     │  │ IOKit/SCSI          │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 命名规范

遵循 Qt 官方命名规范：

| 类型 | 规范 | 示例 |
|------|------|------|
| 类名 | 大驼峰，Q 前缀用于核心库 | `QLtfsIndex`, `TapeDevice` |
| 函数名 | 小驼峰 | `readBlock()`, `writeIndex()` |
| 成员变量 | m_ 前缀 + 小驼峰 | `m_blockSize`, `m_handle` |
| 枚举 | 大驼峰 | `TapeStatus::Ready` |
| 信号 | 小驼峰，动词过去式 | `dataWritten()`, `indexLoaded()` |
| 槽 | 小驼峰，on 前缀 | `onWriteFinished()` |
| 常量 | k 前缀 + 大驼峰 或 全大写下划线 | `kDefaultBlockSize` |

---

## 3. 模块划分与依赖关系

### 3.1 核心库模块（qltfs-core）

此为松耦合的独立函数库，不依赖 Qt Widgets，仅使用 Qt Core。

#### 3.1.1 QLtfsIndex 模块

**功能**：LTFS 索引结构定义、XML 序列化/反序列化

**原项目参考文件**：
- `Schema.vb` - 完整的 LTFS 索引类定义

**主要类**：

```cpp
// LTFS 索引根类
class QLtfsIndex : public QObject {
    Q_OBJECT
public:
    // 属性
    QString creator;
    QUuid volumeUuid;
    quint64 generationNumber;
    QDateTime updateTime;
    IndexLocation location;
    IndexLocation previousGenerationLocation;
    bool allowPolicyUpdate;
    VolumeLockState volumeLockState;
    qint64 highestFileUid;
    
    // 方法
    bool loadFromXml(const QString &xmlContent);
    bool loadFromFile(const QString &filePath);
    QString toXml() const;
    bool saveToFile(const QString &filePath) const;
    
    // 目录和文件列表
    QList<LtfsDirectory*> directories;
    QList<LtfsFile*> files;
};

// LTFS 文件类
class QLtfsFile : public QObject {
    Q_OBJECT
public:
    QString name;
    qint64 length;
    bool readOnly;
    bool openForWrite;
    QDateTime creationTime;
    QDateTime changeTime;
    QDateTime modifyTime;
    QDateTime accessTime;
    QDateTime backupTime;
    qint64 fileUid;
    QString symlink;
    QList<FileExtent> extentInfo;
    QMap<QString, QString> extendedAttributes;
    
    // 哈希相关
    QString getHash(HashType type) const;
    void setHash(HashType type, const QString &value);
};

// LTFS 目录类
class QLtfsDirectory : public QObject {
    Q_OBJECT
public:
    QString name;
    bool readOnly;
    QDateTime creationTime;
    // ... 其他时间属性
    qint64 fileUid;
    QList<QLtfsDirectory*> directories;
    QList<QLtfsFile*> files;
    QList<QLtfsFile*> unwrittenFiles;
    
    qint64 totalFiles() const;
    qint64 totalDirectories() const;
};
```

**参考原代码位置**：
- `Schema.vb:1-300` - `ltfsindex` 类定义
- `Schema.vb:300-600` - `directory` 和 `file` 类定义
- `Schema.vb:600-890` - 序列化方法

#### 3.1.2 QLtfsTape 模块

**功能**：磁带机设备管理、SCSI 命令发送

**原项目参考文件**：
- `TapeUtils.vb` - 核心磁带操作函数
- `LtfsCommand/tape.c` - 底层 SCSI 实现
- `LtfsCommand/tape.h` - 接口定义

**主要类**：

```cpp
// 磁带设备抽象类
class QLtfsTapeDevice : public QObject {
    Q_OBJECT
public:
    // 设备操作
    virtual bool open(const QString &devicePath) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    
    // 磁带操作
    virtual bool load() = 0;
    virtual bool eject() = 0;
    virtual bool rewind() = 0;
    
    // 位置操作
    virtual PositionData readPosition() = 0;
    virtual bool locate(quint64 blockNumber, quint8 partition = 0) = 0;
    virtual bool setPartition(quint8 partition) = 0;
    
    // 数据操作
    virtual QByteArray readBlock(quint32 maxSize = 524288) = 0;
    virtual bool writeBlock(const QByteArray &data) = 0;
    virtual bool writeFileMark(int count = 1) = 0;
    
    // 设备信息
    virtual DeviceInfo inquiry() = 0;
    virtual TapeCapacity readCapacity() = 0;
    
signals:
    void operationStarted();
    void operationFinished();
    void errorOccurred(const QString &error);
};

// 位置数据结构
struct PositionData {
    quint8 partitionNumber;
    quint64 blockNumber;
    quint64 fileNumber;
    bool beginOfPartition;
    bool endOfPartition;
};

// 设备信息结构
struct DeviceInfo {
    QString vendorId;
    QString productId;
    QString serialNumber;
    QString firmwareVersion;
};

// SCSI 命令执行器
class ScsiExecutor {
public:
    static bool sendCommand(
        void *handle,
        const QByteArray &cdb,
        QByteArray &dataBuffer,
        bool dataIn,
        quint32 timeout,
        QByteArray &senseData
    );
};
```

**参考原代码位置**：
- `TapeUtils.vb:1-200` - Windows API 声明
- `TapeUtils.vb:200-400` - SetupAPI 结构定义
- `TapeUtils.vb:400-600` - SCSI 命令构建
- `TapeUtils.vb:600-900` - 设备打开/关闭
- `TapeUtils.vb:900-1500` - 块读写操作
- `tape.c:1-300` - SCSI Pass-Through 实现
- `tape.c:300-600` - 设备枚举

#### 3.1.3 QLtfsIO 模块

**功能**：文件 I/O 操作、哈希计算、格式转换

**原项目参考文件**：
- `IOManager.vb` - 文件操作和哈希计算

**主要类**：

```cpp
// 哈希计算器
class HashCalculator : public QObject {
    Q_OBJECT
public:
    enum HashType {
        SHA1,
        SHA256,
        SHA512,
        MD5,
        CRC32,
        BLAKE3,
        XXHash3,
        XXHash128
    };
    
    static QString calculateFile(const QString &filePath, HashType type);
    static QString calculateData(const QByteArray &data, HashType type);
    
signals:
    void progressChanged(qint64 bytesProcessed, qint64 totalBytes);
};

// 文件操作工具
class FileUtils {
public:
    static QString formatSize(qint64 bytes, bool useDecimal = false);
    static QByteArray hexStringToBytes(const QString &hex);
    static QString bytesToHex(const QByteArray &bytes, bool formatted = false);
};

// 缓冲流读取器
class BufferedStreamReader : public QObject {
    Q_OBJECT
public:
    explicit BufferedStreamReader(const QString &filePath, 
                                   qint64 bufferSize = 65536);
    qint64 read(char *data, qint64 maxSize);
    bool atEnd() const;
    qint64 pos() const;
    qint64 size() const;
    void close();
};
```

**参考原代码位置**：
- `IOManager.vb:1-100` - 格式化函数
- `IOManager.vb:100-300` - 哈希计算实现
- `IOManager.vb:300-600` - 文件读写辅助

#### 3.1.4 QLtfsLabel 模块

**功能**：LTFS 标签结构定义

**原项目参考**：
- `LTFSWriter.vb:24-30` - `ltfslabel` 属性

```cpp
// LTFS 标签类
class QLtfsLabel : public QObject {
    Q_OBJECT
public:
    quint32 blockSize = 524288;  // 默认 512KB
    QString volumeName;
    QUuid volumeUuid;
    
    struct PartitionInfo {
        char indexPartition = 'a';
        char dataPartition = 'b';
    } partitions;
    
    bool loadFromXml(const QString &xmlContent);
    QString toXml() const;
};
```

---

### 3.2 UI 模块（qltfs-gui）

#### 3.2.1 LTFSWriterWindow

**功能**：直接读写主界面

**原项目参考文件**：
- `LTFSWriter.vb` - 主逻辑
- `LTFSWriter.Designer.vb` - UI 布局

**UI 组件**：

```
┌─────────────────────────────────────────────────────────────┐
│ 菜单栏 [磁带] [索引] [数据操作] [帮助]                      │
├─────────────────────────────────────────────────────────────┤
│ 工具栏                                                      │
├────────────────────┬────────────────────────────────────────┤
│                    │                                        │
│   目录树视图       │   文件列表视图                         │
│   (TreeView)       │   (ListView)                           │
│                    │                                        │
│                    ├────────────────────────────────────────┤
│                    │   速度图表                              │
│                    │   (QChartView)                         │
│                    │                                        │
├────────────────────┴────────────────────────────────────────┤
│ 状态栏 [速度] [进度] [剩余时间] [状态指示灯]                 │
└─────────────────────────────────────────────────────────────┘
```

**主要功能**：
1. 显示磁带文件系统结构
2. 拖放添加文件
3. 写入数据到磁带
4. 读取磁带数据
5. 实时速度监控
6. 进度显示

**参考原代码位置**：
- `LTFSWriter.Designer.vb:1-200` - UI 组件定义
- `LTFSWriter.vb:200-500` - 设置加载/保存
- `LTFSWriter.vb:800-1200` - 定时器和状态更新
- `LTFSWriter.vb:1200+` - 文件写入逻辑

#### 3.2.2 FileBrowserDialog

**功能**：文件选择对话框

**原项目参考文件**：
- `FileBrowser.vb` - 逻辑
- `FileBrowser.Designer.vb` - UI 布局

**UI 组件**：

```
┌─────────────────────────────────────────────────────────────┐
│                    文件浏览器                                │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│   ┌─ 树形视图 (带复选框) ──────────────────────────────┐   │
│   │  □ 目录1                                            │   │
│   │    □ 子目录1                                        │   │
│   │      ☑ 文件1.txt                                    │   │
│   │      ☑ 文件2.dat                                    │   │
│   │    □ 子目录2                                        │   │
│   │  □ 目录2                                            │   │
│   └─────────────────────────────────────────────────────┘   │
│                                                              │
│   ☑ 选择时复制信息到剪贴板                                 │
│                                                              │
│   [确定]                                        [取消]      │
├─────────────────────────────────────────────────────────────┤
│ 右键菜单: [全选] [按大小筛选] [按文件名正则匹配]            │
└─────────────────────────────────────────────────────────────┘
```

**主要功能**：
1. 树形显示 LTFS 索引文件结构
2. 复选框选择文件
3. 父子节点联动选择
4. 按条件筛选（大小、正则匹配）

**参考原代码位置**：
- `FileBrowser.vb:1-100` - 初始化和数据绑定
- `FileBrowser.vb:100-200` - 选择状态管理
- `FileBrowser.vb:200-287` - 筛选功能

#### 3.2.3 TreeViewEx

**功能**：扩展的树形视图控件，支持三态复选框

**原项目参考文件**：
- `TreeViewEx.vb` - 自定义 TreeView 控件

---

## 4. 重构步骤详细规划

### 阶段一：项目基础架构 (1-2 周)

#### 步骤 1.1：创建项目结构

**任务**：
- 创建 CMake 项目配置
- 设置多编译器支持
- 配置 Qt6 依赖

**需创建的文件**：
```
QLTOTapeMan/
├── CMakeLists.txt                 # 根 CMake 配置
├── cmake/
│   └── CompilerSettings.cmake     # 编译器配置
├── src/
│   ├── core/
│   │   └── CMakeLists.txt
│   └── gui/
│       └── CMakeLists.txt
└── tests/
    └── CMakeLists.txt
```

**CMakeLists.txt 关键配置**：
```cmake
cmake_minimum_required(VERSION 3.16)
project(QLTOTapeMan VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Xml Charts)

# 核心库（不依赖 Widgets）
add_subdirectory(src/core)

# GUI 应用
add_subdirectory(src/gui)

# 测试
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
```

#### 步骤 1.2：平台抽象层设计

**任务**：
- 设计跨平台 SCSI 接口
- 实现 Windows SPTI 后端
- 实现 Linux sg 后端（后续）
- 实现 macOS IOKit 后端（后续）

**原项目参考**：
- `TapeUtils.vb:287-400` - `IOCtl` 类，SCSI Pass-Through 结构
- `tape.c:450-550` - `ScsiIoControl` 函数

**接口设计**：
```cpp
// src/core/platform/ScsiBackend.h
class IScsiBackend {
public:
    virtual ~IScsiBackend() = default;
    virtual bool sendCommand(
        void *handle,
        const QByteArray &cdb,
        QByteArray &dataBuffer,
        bool dataIn,
        quint32 timeout,
        QByteArray &senseData
    ) = 0;
    virtual void *openDevice(const QString &path) = 0;
    virtual bool closeDevice(void *handle) = 0;
    virtual QStringList enumerateDevices() = 0;
};

// Windows 实现
class WindowsScsiBackend : public IScsiBackend { ... };

// Linux 实现
class LinuxScsiBackend : public IScsiBackend { ... };
```

### 阶段二：核心库开发 (3-4 周)

#### 步骤 2.1：QLtfsIndex 模块实现

**任务**：
- 定义 LTFS 索引数据结构
- 实现 XML 序列化/反序列化
- 实现文件/目录操作方法

**原项目参考**：
- `Schema.vb:1-100` - `ltfsindex` 类属性定义
- `Schema.vb:100-250` - `file` 类定义
- `Schema.vb:250-450` - `directory` 类定义
- `Schema.vb:450-600` - `contentsDef` 定义
- `Schema.vb:600-890` - 序列化方法实现

**实现要点**：
1. 使用 `QXmlStreamReader/Writer` 进行 XML 处理
2. 保持与原 LTFS 标准兼容的 XML 格式
3. 支持扩展属性 (xattr) 存储

**关键方法映射**：

| 原 VB 方法 | Qt/C++ 方法 | 位置参考 |
|------------|-------------|----------|
| `GetSerializedText()` | `QLtfsIndex::toXml()` | Schema.vb:620-700 |
| `SaveFile()` | `QLtfsIndex::saveToFile()` | Schema.vb:700-800 |
| `FromXML()` | `QLtfsIndex::loadFromXml()` | Schema.vb:800-850 |
| `file.GetXAttr()` | `QLtfsFile::getAttribute()` | Schema.vb:150-170 |
| `file.SetXattr()` | `QLtfsFile::setAttribute()` | Schema.vb:170-190 |
| `directory.RefreshCount()` | `QLtfsDirectory::updateCounts()` | Schema.vb:370-420 |

#### 步骤 2.2：QLtfsTape 模块实现

**任务**：
- 实现设备枚举
- 实现 SCSI 命令发送
- 实现磁带操作函数

**原项目参考**：
- `TapeUtils.vb:600-900` - 设备打开/关闭
- `TapeUtils.vb:900-1500` - 块读写操作
- `TapeUtils.vb:1500-2500` - 定位操作
- `TapeUtils.vb:2500-4000` - MAM 属性读写
- `tape.c:30-150` - `TapeGetDriveList()` 设备枚举
- `tape.c:450-550` - SCSI 命令发送

**关键 SCSI 命令实现**：

| 操作 | CDB | 原代码位置 |
|------|-----|------------|
| TEST UNIT READY | `{0x00, ...}` | TapeUtils.vb:725 |
| INQUIRY | `{0x12, ...}` | TapeUtils.vb:780-850 |
| READ POSITION | `{0x34, ...}` | TapeUtils.vb:2100-2200 |
| LOCATE | `{0x92, ...}` | TapeUtils.vb:1700-1800 |
| READ (6) | `{0x08, ...}` | TapeUtils.vb:950-1050 |
| WRITE (6) | `{0x0A, ...}` | TapeUtils.vb:1100-1200 |
| WRITE FILEMARK | `{0x10, ...}` | TapeUtils.vb:1300-1400 |
| LOAD/UNLOAD | `{0x1B, ...}` | TapeUtils.vb:1500-1600 |
| LOG SENSE | `{0x4D, ...}` | TapeUtils.vb:3000-3200 |
| READ ATTRIBUTE | `{0x8C, ...}` | TapeUtils.vb:2700-2900 |

**核心函数映射**：

| 原 VB 函数 | Qt/C++ 函数 | 位置参考 |
|------------|-------------|----------|
| `OpenTapeDrive()` | `TapeDevice::open()` | TapeUtils.vb:550-620 |
| `CloseTapeDrive()` | `TapeDevice::close()` | TapeUtils.vb:620-680 |
| `ReadBlock()` | `TapeDevice::readBlock()` | TapeUtils.vb:900-1000 |
| `WriteBlock()` | `TapeDevice::writeBlock()` | TapeUtils.vb:1100-1180 |
| `ReadPosition()` | `TapeDevice::readPosition()` | TapeUtils.vb:2100-2180 |
| `Locate()` | `TapeDevice::locate()` | TapeUtils.vb:1700-1800 |
| `Inquiry()` | `TapeDevice::inquiry()` | TapeUtils.vb:780-860 |
| `ReadBarcode()` | `TapeDevice::readBarcode()` | TapeUtils.vb:715-720 |
| `LogSense()` | `TapeDevice::readLogPage()` | TapeUtils.vb:3000-3100 |

#### 步骤 2.3：QLtfsIO 模块实现

**任务**：
- 实现多种哈希算法
- 实现文件格式化函数
- 实现缓冲流操作

**原项目参考**：
- `IOManager.vb:34-65` - `FormatSize()` 函数
- `IOManager.vb:100-300` - 哈希计算函数

**哈希实现**：

| 原 VB 函数 | Qt/C++ 函数 | 第三方库 |
|------------|-------------|----------|
| `SHA1()` | `HashCalculator::sha1()` | Qt 内置 |
| `GetSHA256()` | `HashCalculator::sha256()` | Qt 内置 |
| `GetSHA512()` | `HashCalculator::sha512()` | Qt 内置 |
| `GetMD5()` | `HashCalculator::md5()` | Qt 内置 |
| `GetCRC32()` | `HashCalculator::crc32()` | 自实现或第三方 |
| `GetBlake3()` | `HashCalculator::blake3()` | BLAKE3 库 |
| `GetXxHash3()` | `HashCalculator::xxhash3()` | xxHash 库 |

### 阶段三：UI 开发 (4-5 周)

#### 步骤 3.1：主窗口框架

**任务**：
- 创建应用主窗口
- 实现菜单栏
- 实现工具栏

**原项目参考**：
- `LTFSWriter.Designer.vb:1-200` - UI 组件声明
- `LTFSWriter.vb:260-380` - `Load_Settings()` 设置加载

#### 步骤 3.2：LTFSWriter 窗口实现

**任务**：
- 实现目录树视图
- 实现文件列表视图
- 实现拖放功能
- 实现速度图表

**原项目参考**：

**目录树视图**：
- `LTFSWriter.vb:1500-2000` - 树节点操作
- `LTFSWriter.Designer.vb:50-80` - TreeView 定义

**文件列表视图**：
- `LTFSWriter.Designer.vb:80-150` - ListView 定义
- 列定义：name, length, creationtime, fileuid, openforwrite, readonly, changetime, modifytime, accesstime, backuptime, tag, StartBlock, Partition, FriendlyLen, writtenBytes

**速度图表**：
- `LTFSWriter.vb:215-220` - SpeedHistory, FileRateHistory 定义
- `LTFSWriter.vb:850-950` - Timer1_Tick 图表更新

**右键菜单功能**：
- `LTFSWriter.Designer.vb:100-180` - ContextMenuStrip 定义

| 菜单项 | 功能 | 原代码位置 |
|--------|------|------------|
| 提取 | 导出文件到本地 | LTFSWriter.vb:4000+ |
| 校验 | 计算并验证哈希 | LTFSWriter.vb:5000+ |
| 剪切/粘贴 | 文件移动 | LTFSWriter.vb:3000+ |
| 删除 | 删除文件/目录 | LTFSWriter.vb:3500+ |
| 添加文件/目录 | 添加到写入队列 | LTFSWriter.vb:2500+ |

#### 步骤 3.3：FileBrowser 对话框实现

**任务**：
- 实现带复选框的树形视图
- 实现父子联动选择
- 实现筛选功能

**原项目参考**：
- `FileBrowser.vb:1-60` - 初始化和 AddItem
- `FileBrowser.vb:60-100` - AfterSelect 事件
- `FileBrowser.vb:100-170` - 选择状态管理
- `FileBrowser.vb:170-220` - AfterCheck 事件
- `FileBrowser.vb:220-287` - 筛选功能

**关键函数映射**：

| 原 VB 方法 | Qt/C++ 方法 |
|------------|-------------|
| `AddItem()` | `FileBrowserDialog::addItems()` |
| `RecursivelySetNodeCheckStatus()` | `FileBrowserDialog::setCheckStateRecursive()` |
| `RefreshChackState()` | `FileBrowserDialog::updateCheckState()` |
| `按大小ToolStripMenuItem_Click()` | `FileBrowserDialog::filterBySize()` |
| `匹配文件名ToolStripMenuItem_Click()` | `FileBrowserDialog::filterByRegex()` |

#### 步骤 3.4：状态栏实现

**任务**：
- 实现速度显示
- 实现进度条
- 实现状态指示灯
- 实现剩余时间估算

**原项目参考**：
- `LTFSWriter.vb:390-490` - `Timer2_Tick()` 状态更新
- `LTFSWriter.vb:920-1000` - 进度和速度显示

**状态指示灯颜色**（参考 `LTFSWriter.vb:430-470`）：

| 状态 | 颜色 |
|------|------|
| NotReady | Gray |
| Idle | Blue |
| Busy | Orange |
| Success | Green |
| Error | Red |

### 阶段四：核心功能实现 (4-5 周)

#### 步骤 4.1：LTFS 索引读写

**任务**：
- 实现从磁带读取索引
- 实现索引解析
- 实现写入索引到磁带

**原项目参考**：
- `LTFSWriter.vb:2000-2500` - 索引读取逻辑
- `TapeUtils.vb:4000-5000` - LTFS 格式解析

**索引位置约定**：
- 索引区（Partition A）: 块 0 开始
- 数据区（Partition B）: 数据末尾

**读取流程**：
1. 定位到分区起始位置
2. 读取 ANSI Label（80 字节）
3. 读取 VOL1 Label（80 字节）
4. 跳过到索引块
5. 读取并解析 XML 索引

#### 步骤 4.2：数据写入功能

**任务**：
- 实现文件写入队列
- 实现块级写入
- 实现索引更新

**原项目参考**：
- `LTFSWriter.vb:1000-1200` - `FileRecord` 类
- `LTFSWriter.vb:5000-7000` - 写入主循环
- `LTFSWriter.vb:7000-8000` - 索引更新

**写入流程**：
1. 构建待写入文件列表
2. 按顺序读取文件并写入磁带
3. 更新 `extentinfo`（块位置信息）
4. 计算并存储哈希（如启用）
5. 定期更新数据区索引

**FileRecord 类映射**（参考 `LTFSWriter.vb:1000-1200`）：

```cpp
class FileWriteRecord {
public:
    QLtfsDirectory *parentDirectory;
    QString sourcePath;
    QLtfsFile *file;
    QByteArray buffer;
    bool isOpened = false;
    
    bool open(qint64 bufferSize = 65536);
    qint64 read(char *data, qint64 maxSize);
    void close();
    QByteArray readAll();
};
```

#### 步骤 4.3：数据读取功能

**任务**：
- 实现文件提取
- 实现块级读取
- 实现断点续传

**原项目参考**：
- `LTFSWriter.vb:4000-5000` - 文件提取逻辑

### 阶段五：完善与优化 (2-3 周)

#### 步骤 5.1：设置与配置

**任务**：
- 实现用户设置存储
- 实现多语言支持
- 实现主题支持

**原项目参考**：
- `LTFSWriter.vb:260-380` - 设置加载/保存

**设置项映射**：

| 原设置项 | Qt/C++ 设置项 |
|----------|---------------|
| `LTFSWriter_OverwriteExist` | `overwriteExisting` |
| `LTFSWriter_SkipSymlink` | `skipSymlinks` |
| `LTFSWriter_IndexWriteInterval` | `indexWriteInterval` |
| `LTFSWriter_SpeedLimit` | `speedLimit` |
| `LTFSWriter_HashOnWriting` | `calculateHashOnWrite` |
| `LTFSWriter_PreLoadFileCount` | `preloadFileCount` |
| `LTFSWriter_PreLoadBytes` | `preloadBufferSize` |

#### 步骤 5.2：错误处理与日志

**任务**：
- 实现统一错误处理
- 实现日志系统
- 实现 SCSI sense 解析

**原项目参考**：
- `TapeUtils.vb:5000-6000` - `ParseSenseData()` 函数
- `LTFSWriter.vb:470-530` - `PrintMsg()` 日志函数

#### 步骤 5.3：性能优化

**任务**：
- 实现异步 I/O
- 实现内存池
- 实现预读取缓冲

**原项目参考**：
- `LTFSWriter.vb:1150-1200` - 预读取实现
- `IOManager.vb:20-30` - `PublicArrayPool` 内存池

---

## 5. 代码映射参考表

### 5.1 类映射

| 原 VB 类 | Qt/C++ 类 | 文件位置 |
|----------|-----------|----------|
| `ltfsindex` | `QLtfsIndex` | Schema.vb:1-50 |
| `ltfsindex.file` | `QLtfsFile` | Schema.vb:50-250 |
| `ltfsindex.directory` | `QLtfsDirectory` | Schema.vb:250-450 |
| `ltfsindex.extent` | `FileExtent` | Schema.vb:200-230 |
| `ltfsindex.xattr` | 使用 `QMap<QString, QString>` | Schema.vb:120-160 |
| `ltfslabel` | `QLtfsLabel` | LTFSWriter.vb:24-30 |
| `TapeUtils` | `QLtfsTapeDevice` | TapeUtils.vb |
| `TapeUtils.PositionData` | `PositionData` | TapeUtils.vb:2100 |
| `TapeUtils.BlockDevice` | `DeviceInfo` | TapeUtils.vb:730 |
| `TapeUtils.PageData` | `LogPageData` | TapeUtils.vb:3200 |
| `IOManager` | `QLtfsIO` / `HashCalculator` | IOManager.vb |
| `LTFSWriter` | `LTFSWriterWindow` | LTFSWriter.vb |
| `LTFSWriter.FileRecord` | `FileWriteRecord` | LTFSWriter.vb:1000-1200 |
| `FileBrowser` | `FileBrowserDialog` | FileBrowser.vb |
| `TreeViewEx` | `CheckableTreeWidget` | TreeViewEx.vb |

### 5.2 核心函数映射

| 原 VB 函数 | Qt/C++ 函数 | 功能描述 |
|------------|-------------|----------|
| `TapeUtils.ReadBlock()` | `TapeDevice::readBlock()` | 读取磁带块 |
| `TapeUtils.WriteBlock()` | `TapeDevice::writeBlock()` | 写入磁带块 |
| `TapeUtils.Locate()` | `TapeDevice::locate()` | 定位到指定块 |
| `TapeUtils.ReadPosition()` | `TapeDevice::readPosition()` | 读取当前位置 |
| `TapeUtils.ReadBarcode()` | `TapeDevice::readBarcode()` | 读取条码 |
| `TapeUtils.Inquiry()` | `TapeDevice::inquiry()` | 查询设备信息 |
| `TapeUtils.TestUnitReady()` | `TapeDevice::testUnitReady()` | 检测设备就绪 |
| `TapeUtils.LogSense()` | `TapeDevice::readLogPage()` | 读取日志页 |
| `TapeUtils.Load()` | `TapeDevice::load()` | 装载磁带 |
| `TapeUtils.Eject()` | `TapeDevice::eject()` | 弹出磁带 |
| `IOManager.FormatSize()` | `FileUtils::formatSize()` | 格式化大小 |
| `IOManager.SHA1()` | `HashCalculator::sha1()` | 计算 SHA1 |
| `ltfsindex.GetSerializedText()` | `QLtfsIndex::toXml()` | 序列化索引 |
| `ltfsindex.SaveFile()` | `QLtfsIndex::saveToFile()` | 保存索引文件 |

---

## 6. UI 设计参考

### 6.1 LTFSWriter 窗口

**原项目参考**：`LTFSWriter.Designer.vb:1-1717`

**主要控件**：

| 控件类型 | 名称 | 功能 |
|----------|------|------|
| SplitContainer | SplitContainer1 | 主分割面板 |
| TreeView | TreeView1 | 目录树 |
| ListView | ListView1 | 文件列表 |
| Chart | Chart1 | 速度图表 |
| MenuStrip | MenuStrip1 | 菜单栏 |
| StatusStrip | StatusStrip1 | 状态栏 |
| ContextMenuStrip | ContextMenuStrip1 | 文件右键菜单 |
| ContextMenuStrip | ContextMenuStrip3 | 目录右键菜单 |

**菜单结构**：

```
磁带 (MenuStrip)
├── 索引
│   ├── 读取索引
│   ├── 读取数据区索引
│   ├── 加载外部索引
│   └── 备份当前索引
├── 数据操作
│   ├── 写入数据
│   ├── 更新数据区索引
│   └── 更新全部索引
└── 自动化
    ├── 写入后操作
    ├── 电源选项
    └── ...
```

**ListView 列定义**（参考 `LTFSWriter.Designer.vb:80-130`）：

| 列名 | 显示名称 | 宽度 |
|------|----------|------|
| Column_name | 名称 | 200 |
| Column_length | 大小 | 100 |
| Column_creationtime | 创建时间 | 150 |
| Column_fileuid | UID | 60 |
| Column_StartBlock | 起始块 | 80 |
| Column_Partition | 分区 | 50 |
| Column_FriendlyLen | 友好大小 | 100 |
| Column_writtenBytes | 已写入 | 100 |

### 6.2 FileBrowser 对话框

**原项目参考**：`FileBrowser.Designer.vb:1-130`

**控件布局**：

| 控件类型 | 名称 | 功能 |
|----------|------|------|
| TreeViewEx | TreeView1 | 带复选框的树视图 |
| Button | Button1 | 确定按钮 |
| Button | Button2 | 取消按钮 |
| CheckBox | CheckBox1 | 复制信息到剪贴板 |
| ContextMenuStrip | ContextMenuStrip1 | 右键菜单 |

**右键菜单项**：
- 全选
- 按大小筛选
- 按文件名正则匹配

---

## 7. 跨平台兼容性考虑

### 7.1 SCSI 命令发送

| 平台 | 接口 | 实现方式 |
|------|------|----------|
| Windows | SPTI (SCSI Pass-Through Interface) | DeviceIoControl + IOCTL_SCSI_PASS_THROUGH_DIRECT |
| Linux | SG (SCSI Generic) | ioctl(SG_IO) |
| macOS | IOKit | IOServiceGetMatchingServices + SCSITaskDeviceInterface |

**原项目 Windows 实现参考**：
- `TapeUtils.vb:287-400` - SCSI_PASS_THROUGH_DIRECT 结构
- `TapeUtils.vb:400-470` - IOCtlDirect 函数
- `tape.c:450-550` - ScsiIoControl 函数

### 7.2 设备枚举

| 平台 | 方法 |
|------|------|
| Windows | SetupAPI (SetupDiGetClassDevs) |
| Linux | /sys/class/scsi_tape 或 udev |
| macOS | IOServiceMatching |

**原项目 Windows 实现参考**：
- `TapeUtils.vb:10-200` - SetupAPI 声明和结构
- `tape.c:30-150` - TapeGetDriveList 函数

### 7.3 文件路径处理

使用 Qt 的 `QDir` 和 `QFileInfo` 确保跨平台路径处理。

### 7.4 编译器兼容性

**CMake 配置要点**：
```cmake
# 编译器特定设置
if(MSVC)
    add_compile_options(/W4 /WX)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Windows 特定
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE setupapi)
endif()

# Linux 特定
if(UNIX AND NOT APPLE)
    # sg 接口无需额外链接库
endif()

# macOS 特定
if(APPLE)
    find_library(IOKIT_FRAMEWORK IOKit)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${IOKIT_FRAMEWORK})
endif()
```

---

## 8. 测试策略

### 8.1 单元测试

使用 Qt Test 框架。

**核心库测试**：
- `QLtfsIndex` 序列化/反序列化测试
- `HashCalculator` 各算法正确性测试
- `FileUtils` 格式化函数测试

### 8.2 集成测试

- 模拟磁带设备接口测试
- 完整读写流程测试

### 8.3 UI 测试

- 使用 Qt Test 进行 UI 自动化测试
- 手动功能验证

---

## 9. 项目结构规划

```
QLTOTapeMan/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── REFACTORING_PLAN.md          # 本文档
├── .gitignore
│
├── cmake/
│   ├── CompilerSettings.cmake
│   └── FindPackages.cmake
│
├── src/
│   ├── core/                     # qltfs-core 库
│   │   ├── CMakeLists.txt
│   │   ├── qltfs_global.h        # 导出宏定义
│   │   │
│   │   ├── index/                # LTFS 索引模块
│   │   │   ├── qltfsindex.h
│   │   │   ├── qltfsindex.cpp
│   │   │   ├── qltfsfile.h
│   │   │   ├── qltfsfile.cpp
│   │   │   ├── qltfsdirectory.h
│   │   │   ├── qltfsdirectory.cpp
│   │   │   └── qltfslabel.h
│   │   │
│   │   ├── tape/                 # 磁带操作模块
│   │   │   ├── tapedevice.h
│   │   │   ├── tapedevice.cpp
│   │   │   ├── scsicommand.h
│   │   │   ├── scsicommand.cpp
│   │   │   └── positiondata.h
│   │   │
│   │   ├── platform/             # 平台抽象层
│   │   │   ├── scsibackend.h
│   │   │   ├── windows/
│   │   │   │   └── windowsscsibackend.cpp
│   │   │   ├── linux/
│   │   │   │   └── linuxscsibackend.cpp
│   │   │   └── macos/
│   │   │       └── macosscsibackend.cpp
│   │   │
│   │   └── io/                   # I/O 工具模块
│   │       ├── hashcalculator.h
│   │       ├── hashcalculator.cpp
│   │       ├── fileutils.h
│   │       └── fileutils.cpp
│   │
│   └── gui/                      # qltfs-gui 应用
│       ├── CMakeLists.txt
│       ├── main.cpp
│       │
│       ├── mainwindow/
│       │   ├── mainwindow.h
│       │   ├── mainwindow.cpp
│       │   └── mainwindow.ui
│       │
│       ├── ltfswriter/
│       │   ├── ltfswriterwindow.h
│       │   ├── ltfswriterwindow.cpp
│       │   ├── ltfswriterwindow.ui
│       │   ├── filewriterecord.h
│       │   └── filewriterecord.cpp
│       │
│       ├── filebrowser/
│       │   ├── filebrowserdialog.h
│       │   ├── filebrowserdialog.cpp
│       │   └── filebrowserdialog.ui
│       │
│       ├── widgets/
│       │   ├── checkabletreewidget.h
│       │   └── checkabletreewidget.cpp
│       │
│       └── resources/
│           ├── icons/
│           ├── translations/
│           └── resources.qrc
│
├── tests/
│   ├── CMakeLists.txt
│   ├── test_qltfsindex.cpp
│   ├── test_hashcalculator.cpp
│   └── test_fileutils.cpp
│
├── docs/
│   ├── api/
│   └── user_guide/
│
└── ref_LTFSCopyGUI/              # 原项目参考（已在 .gitignore 中排除）
```

---

## 附录 A：LTFS 标准参考

### A.1 LTFS 索引 XML 结构

```xml
<?xml version="1.0" encoding="UTF-8"?>
<ltfsindex version="2.4.0">
    <creator>QLTOTapeMan 1.0.0</creator>
    <volumeuuid>xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx</volumeuuid>
    <generationnumber>1</generationnumber>
    <updatetime>2024-01-01T00:00:00.0000000Z</updatetime>
    <location>
        <partition>a</partition>
        <startblock>5</startblock>
    </location>
    <directory>
        <name>root</name>
        <readonly>false</readonly>
        <creationtime>...</creationtime>
        <contents>
            <file>
                <name>example.txt</name>
                <length>1024</length>
                <fileuid>1</fileuid>
                <extentinfo>
                    <extent>
                        <partition>b</partition>
                        <startblock>100</startblock>
                        <byteoffset>0</byteoffset>
                        <bytecount>1024</bytecount>
                    </extent>
                </extentinfo>
            </file>
        </contents>
    </directory>
</ltfsindex>
```

### A.2 LTFS 标签格式

**VOL1 Label（80 字节）**：
- 位置：分区起始
- 格式：ANSI 标准磁带标签

**LTFS Label**：
- 位置：VOL1 之后
- 格式：XML
- 包含：blocksize, volumeuuid, partitions

---

## 附录 B：第三方库依赖

| 库名称 | 用途 | 许可证 |
|--------|------|--------|
| Qt 6.x | UI 框架、XML 处理 | LGPL/Commercial |
| BLAKE3 | BLAKE3 哈希算法 | CC0/Apache 2.0 |
| xxHash | xxHash 算法 | BSD-2-Clause |
| OpenSSL (可选) | 加密功能 | Apache 2.0 |

---

## 附录 C：术语表

| 术语 | 解释 |
|------|------|
| LTFS | Linear Tape File System，线性磁带文件系统 |
| LTO | Linear Tape-Open，开放线性磁带 |
| SCSI | Small Computer System Interface |
| SPTI | SCSI Pass-Through Interface |
| CDB | Command Descriptor Block，命令描述块 |
| MAM | Medium Auxiliary Memory，介质辅助存储器 |
| Partition | 磁带分区（索引区/数据区） |
| Block | 磁带数据块 |
| Filemark | 文件标记 |
| Sense Data | SCSI 错误信息数据 |

---

*文档版本: 1.0*  
*创建日期: 2026-01-14*  
*作者: QLTOTapeMan 开发团队*
