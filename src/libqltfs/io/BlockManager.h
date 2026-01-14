/**
 * QLTOTapeMan - Qt-based LTO Tape Manager
 * Block Manager Header
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 */

#ifndef QLTFS_BLOCKMANAGER_H
#define QLTFS_BLOCKMANAGER_H

#include "../libqltfs_global.h"

#include <QByteArray>
#include <QString>
#include <QVector>
#include <QMutex>

namespace qltfs {

/**
 * @brief Block descriptor for tape I/O
 */
struct LIBQLTFS_EXPORT BlockDescriptor {
    uint64_t blockNumber = 0;       ///< Logical block number
    uint32_t blockSize = 0;         ///< Size of block in bytes
    uint8_t partition = 0;          ///< Partition number (0 or 1)
    bool isFilemark = false;        ///< True if this is a filemark
    bool isEndOfData = false;       ///< True if end of data marker
};

/**
 * @brief Block allocation info
 */
struct LIBQLTFS_EXPORT BlockAllocation {
    uint64_t startBlock = 0;        ///< Starting block number
    uint64_t blockCount = 0;        ///< Number of blocks
    uint8_t partition = 0;          ///< Partition number
    uint64_t fileOffset = 0;        ///< Offset within file
    uint64_t byteCount = 0;         ///< Total bytes in this extent
};

/**
 * @brief Manages block allocation and tracking for LTFS tape operations
 *
 * Handles block-level operations including:
 * - Block size management
 * - Block allocation tracking
 * - Buffer management for read/write operations
 * - Extent tracking
 */
class LIBQLTFS_EXPORT BlockManager
{
public:
    /**
     * @brief Default block size (512 KB)
     */
    static constexpr uint32_t DEFAULT_BLOCK_SIZE = 512 * 1024;
    
    /**
     * @brief Maximum block size (4 MB)
     */
    static constexpr uint32_t MAX_BLOCK_SIZE = 4 * 1024 * 1024;
    
    /**
     * @brief Minimum block size (4 KB)
     */
    static constexpr uint32_t MIN_BLOCK_SIZE = 4 * 1024;

    /**
     * @brief Constructor
     * @param blockSize Block size to use (default 512KB)
     */
    explicit BlockManager(uint32_t blockSize = DEFAULT_BLOCK_SIZE);
    ~BlockManager();

    // Prevent copying
    BlockManager(const BlockManager&) = delete;
    BlockManager& operator=(const BlockManager&) = delete;

    // === Configuration ===

    /**
     * @brief Set block size
     * @param size Block size in bytes
     * @return true if valid size
     */
    bool setBlockSize(uint32_t size);

    /**
     * @brief Get current block size
     */
    uint32_t blockSize() const { return m_blockSize; }

    /**
     * @brief Set current partition
     */
    void setPartition(uint8_t partition) { m_currentPartition = partition; }

    /**
     * @brief Get current partition
     */
    uint8_t partition() const { return m_currentPartition; }

    // === Block Position Tracking ===

    /**
     * @brief Set current block position
     * @param blockNumber Logical block number
     */
    void setBlockPosition(uint64_t blockNumber);

    /**
     * @brief Get current block position
     */
    uint64_t blockPosition() const { return m_currentBlock; }

    /**
     * @brief Advance block position
     * @param blocks Number of blocks to advance
     */
    void advanceBlocks(uint64_t blocks);

    // === Buffer Management ===

    /**
     * @brief Allocate a block buffer
     * @return Pointer to buffer (owned by BlockManager)
     */
    QByteArray allocateBuffer();

    /**
     * @brief Allocate multiple block buffers
     * @param count Number of buffers
     * @return Vector of buffers
     */
    QVector<QByteArray> allocateBuffers(int count);

    /**
     * @brief Get recommended buffer count for streaming
     */
    int recommendedBufferCount() const;

    // === Extent Tracking ===

    /**
     * @brief Start a new extent
     * @return Extent ID
     */
    int beginExtent();

    /**
     * @brief Add blocks to current extent
     * @param blockCount Number of blocks written
     * @param byteCount Actual bytes written
     */
    void addToExtent(uint64_t blockCount, uint64_t byteCount);

    /**
     * @brief End current extent
     * @return BlockAllocation for the extent
     */
    BlockAllocation endExtent();

    /**
     * @brief Get all recorded extents
     */
    QVector<BlockAllocation> extents() const { return m_extents; }

    /**
     * @brief Clear all extents
     */
    void clearExtents();

    // === Statistics ===

    /**
     * @brief Get total blocks written
     */
    uint64_t totalBlocksWritten() const { return m_totalBlocksWritten; }

    /**
     * @brief Get total bytes written
     */
    uint64_t totalBytesWritten() const { return m_totalBytesWritten; }

    /**
     * @brief Reset statistics
     */
    void resetStatistics();

    // === Utility ===

    /**
     * @brief Calculate blocks needed for given byte count
     * @param bytes Number of bytes
     * @return Number of blocks needed
     */
    uint64_t bytesToBlocks(uint64_t bytes) const;

    /**
     * @brief Calculate bytes for given block count
     * @param blocks Number of blocks
     * @return Maximum bytes that fit in blocks
     */
    uint64_t blocksToBytes(uint64_t blocks) const;

    /**
     * @brief Validate block size is within limits
     * @param size Block size to validate
     * @return true if valid
     */
    static bool isValidBlockSize(uint32_t size);

private:
    uint32_t m_blockSize;           ///< Current block size
    uint8_t m_currentPartition;     ///< Current partition
    uint64_t m_currentBlock;        ///< Current block position
    
    // Extent tracking
    BlockAllocation m_currentExtent;
    bool m_extentActive;
    QVector<BlockAllocation> m_extents;
    
    // Statistics
    uint64_t m_totalBlocksWritten;
    uint64_t m_totalBytesWritten;
    
    mutable QMutex m_mutex;
};

} // namespace qltfs

#endif // QLTFS_BLOCKMANAGER_H
