# 阶段 8：平台特性与权限处理（规划）

目标：定义跨平台的权限/提权提示、调试输出与诊断日志策略；仅文档。

## 参考源（原项目）
- `ApplicationEvents.vb`：UAC 自提权（runas）、控制台附着/释放。
- `LTFSConfigurator.vb`：调试面板（SCSI 直发、LogSense 导出）与日志输出框。
- `TapeUtils.vb`：Sense 解析、LogSense/MAM 导出。

## 权限策略
- Windows：
  - 检查管理员；不足时提示“以管理员运行”并可选用 manifest/UAC 弹窗；不强制自动提权。
- Linux：
  - 检测是否 root 或具备 CAP_SYS_RAWIO；不足时友好错误并提示 `sudo` / udev 规则。
- macOS：
  - 检测权限，若实现受限，提示“不支持或需额外权限/驱动”。

## 调试/诊断
- 调试面板：
  - 发送 SCSI CDB，显示 sense；读/写参数缓冲区；与原 `ButtonDebugSendSCSICommand` 对齐。
- LogSense 导出：
  - 允许选择页面/子页，保存为文件（后续复刻 `LTFSConfigurator` 的 Log 页面导出）。
- 日志：
  - 支持文件输出（会话时间戳）与 UI 文本框；可配置最大行数；默认 UTF-8。

## 错误呈现
- 标准格式：`[time] [level] message [sense(optional)]`
- 典型错误文案：权限不足、设备不存在、命令超时、未实现的平台后端。

## 运行时提示
- 若当前平台未编译对应后端（Win/Linux/mac），运行时提示“当前构建未包含 <platform> 设备实现”。
