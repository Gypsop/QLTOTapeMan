# 阶段 4：高层服务与任务流（规划）

目标：设计 `LtfsService` 及其任务流（加载标签/索引、写入、索引周期刷新、容量刷新、退出/退带），定义回调与状态模型；仅文档。

## 范围与输出
- `core/ltfs_service.{h,cpp}` 设计（接口/回调/状态）。
- WriteOptions/CapacityInfo/Status 枚举。
- 错误/日志统一输出格式。

## 参考源（原项目）
- `LTFSWriter.vb`：
  - 入口：加载驱动/索引（在线/离线），`LoadIndexFile`，`LoadLTFS`（读取 label/index），`ExtraPartitionCount` 设置。
  - 写入流程：文件队列 -> `FileDataProvider` -> 写块/写文件标记；`HashOnWrite`；`SpeedLimit`；`IndexWriteInterval`；`CapacityRefreshInterval`；`AutoDump`。
  - 状态灯 `SetStatusLight(LWStatus)`；进度/剩余时间估算；Pause/Stop/Flush；EOD 检测；CleanCycle/AutoClean。
  - 事件：`LTFSLoaded`、`WriteFinished`、`TapeEjected`。
- `FileDataProvider.vb`：背压/小文件缓存，requireSignal。
- `TapeCopy.vb`：块复制 + EOD/sense 处理。
- `TapeUtils.vb`：`ReadPosition`、`Flush`、`WriteFileMark`、`LoadEject`。

## LtfsService 设计要点
- 依赖 `TapeDevice`（已 attach）和 `ltfs_label/index` 模型。
- 回调/信号（可注入）：log(message)，progress(0..1)，status(text)，sense(SenseData)，capacity(CapacityInfo)。
- 核心方法（示意）：
  - `bool load_label(Label&, std::string& err);`
  - `bool load_index(Index&, bool offline, std::string& err);` // offline: 从文件或缓存
  - `bool write_files(const std::vector<FileRecord>& files, const WriteOptions&, std::string& err);`
  - `bool write_index(bool force, std::string& err);`
  - `bool refresh_capacity(CapacityInfo&, std::string& err);`
  - `bool eject(std::string& err);`
  - `bool flush(std::string& err);`
- WriteOptions：block_len、hash_on_write、speed_limit_mib_s、index_interval_bytes、capacity_interval_sec、clean_cycle、extra_partition_count、offline_mode、encryption_key、auto_dump_path、force_index.
- 状态枚举：NotReady / Busy / Succ / Err / Pause / Stopped。

## 任务流（高层）
1) **加载阶段**：打开驱动 -> 读 label -> 读 index（或离线加载）-> 填充模型。
2) **写入阶段**：
   - 建立 `FileDataProvider`；迭代 FileRecord（选择好的项）。
   - 写块/文件标记；更新 bytes/files processed；限速控制；哈希（可选）。
   - 定期写索引（index interval 或手动触发）。
   - 自动刷新容量/位置（capacity interval）。
   - 错误处理：sense != 0 时转换文本，回调日志；可配置 fail-fast 或弹窗策略（UI 处理）。
3) **收尾阶段**：Flush -> 写索引（如果需要）-> AutoDump -> Eject（可选）。

## 容量/位置
- `ReadPosition` 映射 `TapeUtils.ReadPosition` 返回 block/partition；用于 UI 显示与日志。
- `CapacityInfo`：剩余容量、已用、百分比；刷新间隔可配置。

## Hash/限速
- Hash：Blake3（可选）；每文件计算并记录；UI 可显示速度与哈希进度。
- 限速：窗口移动平均，sleep/节流；复刻 `SpeedLimit` 逻辑。

## Clean/Auto behaviors
- AutoClean: 基于最近增量速率与阈值（参考 `TapeCopy` `FlushCounter` 逻辑）。
- AutoDump: 写完后导出 schema + 可选 CM 报告（若支持）。

## 错误模型
- 所有接口返回 bool；err string；附带 sense（若有）。
- 常见错误：设备未打开、权限不足、超时、索引无效、写失败/EOD、哈希失败、限速计时器异常。
