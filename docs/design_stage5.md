# 阶段 5：写入管线与缓存（规划）

目标：设计 Qt/C++ 版 `FileDataProvider` 与 `FileRecord`，保持“小文件缓存 + 背压 + 限速”特性，对齐原 VB 实现；仅文档。

## 参考源（原项目）
- `FileDataProvider.vb`：Pipe 背压阈值 256MiB，小文件缓存阈值 16KiB，缓存容量 1000；requireSignal 模式；RingBuffer 选项；取消/完成流程。
- `LTFSWriter.FileRecord`：预读、xattr 读取、错误重试（Abort/Retry/Ignore）、时间戳填充、.xattr 额外属性；`ReadAllBytes`；`Open` 带重试。
- `LTFSWriter` 写入循环：限速、哈希、进度更新、EOD 检测。

## FileRecord 设计
- 字段：源路径、目标 ltfsindex.file 引用、buffer（可选）、is_opened、fs 句柄、预读 offset/len、xattr 列表。
- 方法：`open(buffer_size)`, `read_all_bytes()`, `close()`, `reset()`；错误重试策略（返回 code -> 上层 UI 决定）。
- 安全路径：支持 `\\?\` 前缀（Windows 长路径），保持原逻辑。

## FileDataProvider 设计
- 模式：
  - 积极模式（默认）：连续推进文件。
  - requireSignal 模式：需外部 `RequestNextFile()` 才推进。
- 背压：
  - 环形缓冲（自实现）或 Qt `QIODevice` + 手动背压；阈值默认 256 MiB，可配置。
- 小文件缓存：
  - 阈值 16 KiB 默认，可配置；缓存容量 1000；超出队列等待。
- 限速钩子：
  - 记录写入字节与时间；超限则 sleep/节流。
- 取消/完成：
  - `Cancel()` 触发 cts；`CompleteAsync()` 收尾并关闭 writer/reader。

## 线程模型
- 生产者线程：预读小文件/流式读取大文件 -> 写入管线。
- 消费者：`LtfsService` 写入循环从管线读取并写入设备。
- 同步：取消/完成时唤醒等待线程；避免死锁。

## 错误处理
- 读取失败：返回错误码，按原逻辑支持 Abort/Retry/Ignore（上层 UI 决定）。
- 超时/取消：抛出/返回特殊状态，LtfsService 处理。

## 配置参数（可暴露给 UI 设置）
- `pipe_buffer_bytes` (default 256<<20)
- `small_threshold_bytes` (default 16*1024)
- `small_cache_capacity` (default 1000)
- `require_signal` (default false)
- `minimum_segment_size` (对应原设置 `LTFSWriter_MinimumSegmentSize`)
