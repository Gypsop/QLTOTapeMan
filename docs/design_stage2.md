# 阶段 2：基础模型与工具层（规划）

目标：定义 ltfs_label / ltfs_index 数据结构、序列化/反序列化、排序/选择状态、xattr 处理，以及可选的哈希/压缩接口占位。此阶段仅出设计，不写代码。

## 范围与输出
- `core/ltfs_label.{h,cpp}`：卷标签、分区、块大小、额外分区、WORM、加密参数。
- `core/ltfs_index.{h,cpp}`：目录树/文件节点、xattr、序列化/反序列化、排序、选择状态。
- `core/hash/`（可选占位）：Blake3、CRC/SHA 接口；Zstd（可选）。

## 参考源（原项目）
- `LTFSCopyGUI/LTFSWriter.vb`：`ltfsindex` 结构的读写、`LoadIndexFile`、`AutoDump`、xattr 读取、时间戳格式、分区字段 (`schema.location.partition`, `startblock`, `generationnumber`, `volumeuuid`, `barcodematched`, `backuptime` 等)。
- `LTFSCopyGUI/TapeUtils.vb`：`mkltfs` 参数（blocksize、label、partition）、`MAMAttribute` 获取条码、VCI；索引写入前后的 label/index 关系。

## 结构要点
- Label：
  - 字段：`barcode`、`vol_label`、`blocksize`、`partitions{index,data,extra}`、`generation`、`encryption_key`(optional)、`worm` 标记。
  - 方法：从磁带读取/生成；与 `mkltfs` 调用参数对齐。
- Index：
  - `directory`/`file` 节点：name、uid、length、time（creation/modify/access/change/backup）、xattr、selected 标记、只读标记。
  - 位置：`location.partition`，`startblock`；`generationnumber`。
  - 序列化：读写 `.schema`（LTFS XML/文本）——保持与原 `ltfsindex.FromSchFile/SaveFile` 兼容。
  - 排序/选择：与 `ltfsindex.WSort` 行为一致；支持全选/正则/按大小筛选（供 FileBrowser 使用）。
- 时间与编码：
  - 统一 UTC ISO-8601 纳秒精度：`yyyy-MM-ddTHH:mm:ss.fffffff00Z`（参考 VB 实现）。
- xattr：
  - 读取/写入 `.xattr` 辅助文件的兼容性；保留键值对列表。

## API 草案（示意）
- `bool load_schema(const std::string& path, Index& out, std::string& err);`
- `bool save_schema(const Index& idx, const std::string& path, std::string& err);`
- `void sort_index(Index&, std::function<bool(const file&, const file&)>);`
- `void mark_selected(Index&, predicate);`

## 校验与转换
- 校验 blocksize、partition 数、generation 自增逻辑。
- 字符编码：假定 UTF-8；必要时处理非法字符替换为 `_`（参考 AutoDump 文件名安全处理）。

## 后续依赖
- 阶段 4（LtfsService）将依赖 label/index API。
- 阶段 6（UI）使用选择状态与元数据展示。
