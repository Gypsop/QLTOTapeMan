#pragma once

#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <cstdint>
#include <atomic>

#include <QString>

struct TransferBlock {
    std::vector<uint8_t> buffer;
    size_t validSize = 0;
    
    enum Type {
        DATA,
        FILE_START,
        FILE_END
    } type = DATA;
    
    // Metadata for FILE_START / FILE_END
    QString fileName;
    quint64 fileSize = 0;
    QString checksum; // For FILE_END
};

class RingBuffer {
public:
    RingBuffer(size_t poolSize, size_t blockSize);
    ~RingBuffer();

    // Producer: Get a block to write data into
    TransferBlock* acquireEmptyBlock();
    
    // Producer: Push a filled block into the queue
    void pushFilledBlock(TransferBlock* block);

    // Consumer: Get a filled block to write to tape
    TransferBlock* acquireFilledBlock();

    // Consumer: Return the block to the empty pool
    void releaseBlock(TransferBlock* block);

    void setFinished();
    bool isFinished() const;
    void reset();
    
    size_t blockSize() const { return m_blockSize; }
    size_t capacity() const { return m_poolSize; }
    size_t filledCount() const;

private:
    size_t m_poolSize;
    size_t m_blockSize;

    std::vector<TransferBlock*> m_allBlocks; // Owner of memory
    std::deque<TransferBlock*> m_emptyQueue;
    std::deque<TransferBlock*> m_filledQueue;

    std::mutex m_mutex;
    std::condition_variable m_condEmpty;  // Wait for empty space
    std::condition_variable m_condFilled; // Wait for data

    std::atomic<bool> m_finished{false};
};
