#include "RingBuffer.h"

RingBuffer::RingBuffer(size_t poolSize, size_t blockSize)
    : m_poolSize(poolSize), m_blockSize(blockSize)
{
    // Pre-allocate blocks
    for (size_t i = 0; i < poolSize; ++i) {
        TransferBlock* block = new TransferBlock();
        block->buffer.resize(blockSize);
        block->validSize = 0;
        block->type = TransferBlock::DATA;
        
        m_allBlocks.push_back(block);
        m_emptyQueue.push_back(block);
    }
}

RingBuffer::~RingBuffer()
{
    for (TransferBlock* block : m_allBlocks) {
        delete block;
    }
    m_allBlocks.clear();
}

TransferBlock* RingBuffer::acquireEmptyBlock()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_condEmpty.wait(lock, [this]() {
        return !m_emptyQueue.empty();
    });

    TransferBlock* block = m_emptyQueue.back();
    m_emptyQueue.pop_back();
    return block;
}

void RingBuffer::pushFilledBlock(TransferBlock* block)
{
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_filledQueue.push_back(block);
    }
    m_condFilled.notify_one();
}

TransferBlock* RingBuffer::acquireFilledBlock()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_condFilled.wait(lock, [this]() {
        return !m_filledQueue.empty() || m_finished;
    });

    if (m_filledQueue.empty() && m_finished) {
        return nullptr;
    }

    TransferBlock* block = m_filledQueue.front();
    m_filledQueue.pop_front();
    return block;
}

void RingBuffer::releaseBlock(TransferBlock* block)
{
    block->validSize = 0;
    block->type = TransferBlock::DATA;
    block->fileName.clear();
    block->checksum.clear();
    
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_emptyQueue.push_back(block);
    }
    m_condEmpty.notify_one();
}

void RingBuffer::setFinished()
{
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_finished = true;
    }
    m_condFilled.notify_all();
}

bool RingBuffer::isFinished() const
{
    return m_finished;
}

void RingBuffer::reset()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_finished = false;
    
    // Move all filled blocks back to empty
    for (TransferBlock* block : m_filledQueue) {
        block->validSize = 0;
        m_emptyQueue.push_back(block);
    }
    m_filledQueue.clear();
}

size_t RingBuffer::filledCount() const
{
    // This is just an estimate, not locked
    return m_filledQueue.size();
}
