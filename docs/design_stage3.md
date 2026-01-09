# 阶段 3：设备抽象与平台实现（规划）

目标：设计 TapeDevice 平台实现策略（Win/Linux/macOS），枚举器，路径/权限处理；仅文档，不写代码。

## 范围与输出
- 平台实现规范：`TapeDeviceWin` / `TapeDeviceSg` / `TapeDeviceMac`。
- 枚举器 `TapeEnumerator` 设计：设备列表、序列号、友好名。
- 权限与路径策略。

## 参考源（原项目）
- `LTFSCopyGUI/TapeUtils.vb`：
  - `GetTapeDriveList`（序列号、vendor/product、DevIndex、DevicePath）
  - `OpenTapeDrive`、`ReadBlock`、`Write`、`WriteFileMark`、`LoadEject`、`ReadPosition`、`LogSense`、`MAMAttribute`、`mkltfs`、`IOCtlDirect`、`SCSIOperationLock`
  - Sense 解析、DriverType、BlockLimit、AllowPartition
- `LTFSConfigurator.vb`：装载/退带/挂载/映射按钮流程，设备路径输入框，自动刷新。

## Windows (TapeDeviceWin)
- API：`CreateFile` + `DeviceIoControl(SCSI_PASS_THROUGH(_DIRECT))`。
- 路径：`\\.\TAPE0` / `\\.\GLOBALROOT\Device\...`；兼容数字/名称输入。
- 权限：需要管理员；UAC 提示（应用层处理）。
- Sense：解析成文本/代码返回。

## Linux (TapeDeviceSg)
- 设备：`/dev/sg*`（pass-through），顺序读写可用 `/dev/nst*`。
- API：`ioctl(SG_IO)`，必要时 `MTIOCGET` 读状态。
- 权限：root 或 CAP_SYS_RAWIO；不足时返回友好错误。
- 阻塞/超时：SG_IO timeout 传入；大块读写分片。

## macOS (TapeDeviceMac)
- 设备：IOKit SCSI/StorageFamily；若实现受限，允许 stub，并在运行时报“不支持该平台”。
- 后续可选：使用 `IOSCSITaskDeviceInterface`。

## 公共策略
- 线程安全：内部互斥，仿 `SCSIOperationLock`。
- Block size：允许上层指定；若设备返回限制需校验。
- Load/Eject：支持 threaded/unthreaded 选项（与原按钮对应）。
- LogSense/MAM：可选实现；若不支持，返回明确错误码。
- mkltfs：平台调用封装；若平台缺失，提示不支持。

## 枚举器设计
- Windows：SetupAPI + GUID_DEVINTERFACE_TAPE；收集 Vendor/Product/Serial/DevIndex；友好名/驱动描述。
- Linux：遍历 `/sys/class/scsi_tape`、`/dev/sg*`，读取 `vendor`, `model`, `serial`（若可）。
- macOS：IORegistry 查询 IOService。
- 输出 `BlockDevice`：`vendor`, `product`, `serial`, `device_path`, `display_name`, `index`。

## 错误与日志
- 接口统一返回 `(bool, err string, sense data)`。
- 常见错误：权限不足、设备不存在、命令超时、sense key 非 0。

## 与后续的关系
- 阶段 4 `LtfsService` 将依赖这些实现。
- 阶段 8 权限提示/自提权逻辑需结合平台特性。
