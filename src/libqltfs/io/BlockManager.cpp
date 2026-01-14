/**
 * QLTOTapeMan - Qt-based LTO Tape Manager
 * Block Manager Implementation
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 */

#include "BlockManager.h"

#include <QDebug>

namespace qltfs {

// =============================================================================
// Constructor / Destructor
// =============================================================================

BlockManager::BlockManager(uint32_t blockSize)
    : m_blockSize(blockSize)
    , m_currentPartition(0)
    , m_currentBlock(0)
    , m_extentActive(false)
    , m_totalBlocksWritten(0)
    , m_totalBytesWritten(0)
{
    if (!isValidBlockSize(blockSize)) {
        m_blockSize = DEFAULT_BLOCK_SIZE;
        qWarning() << "Invalid block size" << blockSize << "using default" << m_blockSize;
    }
}

BlockManager::~BlockManager()
{
}

// =============================================================================
// Configuration
// =============================================================================

bool BlockManager::setBlockSize(uint32_t size)
{
    if (!isValidBlockSize(size)) {
        return false;
    }
    
    QMutexLocker locker(&m_mutex);
    m_blockSize = size;
    return true;
}

// =============================================================================
// Block Position Tracking
// =============================================================================

void BlockManager::setBlockPosition(uint64_t blockNumber)
{
    QMutexLocker locker(&m_mutex);
    m_currentBlock = blockNumber;
}

void BlockManager::advanceBlocks(uint64_t blocks)
{
    QMutexLocker locker(&m_mutex);
    m_currentBlock += blocks;
}

// =============================================================================
// Buffer Management
// =============================================================================

QByteArray BlockManager::allocateBuffer()
{
    return QByteArray(static_cast<int>(m_blockSize), '\0');
}

QVector<QByteArray> BlockManager::allocateBuffers(int count)
{
    QVector<QByteArray> buffers;
    buffers.reserve(count);
    
    for (int i = 0; i < count; ++i) {
        buffers.append(allocateBuffer());
    }
    
    return buffers;
}

int BlockManager::recommendedBufferCount() const
{
    // Recommend enough buffers for 32 MB of streaming
    constexpr uint64_t targetBytes = 32 * 1024 * 1024;
    int count = static_cast<int>(targetBytes / m_blockSize);
    
    // Clamp between 4 and 64 buffers
    return qBound(4, count, 64);
}

// =============================================================================
// Extent Tracking
// =============================================================================

int BlockManager::beginExtent()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_extentActive) {
        // End previous extent
        locker.unlock();
        endExtent();
        locker.relock();
    }
    
    m_currentExtent = BlockAllocation();
    m_currentExtent.startBlock = m_currentBlock;
    m_currentExtent.partition = m_currentPartition;
    m_currentExtent.fileOffset = 0;
    m_extentActive = true;
    
    return static_cast<int>(m_extents.size());
}

void BlockManager::addToExtent(uint64_t blockCount, uint64_t byteCount)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_extentActive) {
        return;
    }
    
    m_currentExtent.blockCount += blockCount;
    m_currentExtent.byteCount += byteCount;
    m_currentBlock += blockCount;
    
    m_totalBlocksWritten += blockCount;
    m_totalBytesWritten += byteCount;
}

BlockAllocation BlockManager::endExtent()
{
    QMutexLocker locker(&m_mutex);
    
    BlockAllocation result = m_currentExtent;
    
    if (m_extentActive) {
        m_extents.append(m_currentExtent);
        m_extentActive = false;
        m_currentExtent = BlockAllocation();
    }
    
    return result;
}

void BlockManager::clearExtents()
{
    QMutexLocker locker(&m_mutex);
    m_extents.clear();
    m_extentActive = false;
    m_currentExtent = BlockAllocation();
}

// =============================================================================
// Statistics
// =============================================================================

void BlockManager::resetStatistics()
{
    QMutexLocker locker(&m_mutex);
    m_totalBlocksWritten = 0;
    m_totalBytesWritten = 0;
}

// =============================================================================
// Utility
// =============================================================================

uint64_t BlockManager::bytesToBlocks(uint64_t bytes) const
{
    if (m_blockSize == 0) {
        return 0;
    }
    return (bytes + m_blockSize - 1) / m_blockSize;
}

uint64_t BlockManager::blocksToBytes(uint64_t blocks) const
{
    return blocks * m_blockSize;
}

bool BlockManager::isValidBlockSize(uint32_t size)
{
    // Must be within limits
    if (size < MIN_BLOCK_SIZE || size > MAX_BLOCK_SIZE) {
        return false;
    }
    
    // Must be power of 2 or multiple of 4KB
    if ((size & (size - 1)) == 0) {
        return true; // Power of 2
    }
    
    return (size % MIN_BLOCK_SIZE) == 0; // Multiple of 4KB
}

} // namespace qltfs
