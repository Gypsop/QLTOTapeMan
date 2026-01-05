#include "TransferEngine.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QCryptographicHash>
#include <QElapsedTimer>

// Buffer Configuration
static const size_t BUFFER_POOL_SIZE = 512; // 512 blocks
static const size_t BLOCK_SIZE = 1024 * 1024; // 1MB per block -> 512MB Total Buffer

TransferEngine::TransferEngine(QObject *parent) : QObject(parent)
{
    m_buffer = new RingBuffer(BUFFER_POOL_SIZE, BLOCK_SIZE);
}

TransferEngine::~TransferEngine()
{
    stop();
    if (m_readerThread.joinable()) m_readerThread.join();
    if (m_writerThread.joinable()) m_writerThread.join();
    delete m_buffer;
}

void TransferEngine::setSourceFiles(const QStringList& files)
{
    m_sourceFiles = files;
    m_totalBytes = 0;
    for (const QString& f : files) {
        m_totalBytes += QFileInfo(f).size();
    }
}

void TransferEngine::setDestinationPath(const QString& path)
{
    m_destPath = path;
}

void TransferEngine::setDestinationDevice(const QString& devicePath)
{
    m_destDevice = devicePath;
}

void TransferEngine::setDeviceManager(DeviceManager* deviceManager)
{
    m_deviceManager = deviceManager;
}

void TransferEngine::start()
{
    if (m_running) return;
    
    m_running = true;
    m_abort = false;
    m_processedBytes = 0;
    m_buffer->reset();
    
    m_readerThread = std::thread(&TransferEngine::readerLoop, this);
    m_writerThread = std::thread(&TransferEngine::writerLoop, this);
}

void TransferEngine::stop()
{
    if (!m_running) return;
    m_abort = true;
    m_buffer->setFinished(); // Wake up consumers
}

void TransferEngine::readerLoop()
{
    for (const QString& filePath : m_sourceFiles) {
        if (m_abort) break;

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            emit errorOccurred("Failed to open source file: " + filePath);
            continue;
        }

        QFileInfo fileInfo(filePath);
        QString fileName = fileInfo.fileName();
        quint64 fileSize = fileInfo.size();

        // 1. Send FILE_START
        TransferBlock* startBlock = m_buffer->acquireEmptyBlock();
        startBlock->type = TransferBlock::FILE_START;
        startBlock->fileName = fileName;
        startBlock->fileSize = fileSize;
        m_buffer->pushFilledBlock(startBlock);

        // 2. Read Data
        QCryptographicHash hash(QCryptographicHash::Sha1);
        
        while (!file.atEnd() && !m_abort) {
            TransferBlock* dataBlock = m_buffer->acquireEmptyBlock();
            qint64 bytesRead = file.read((char*)dataBlock->buffer.data(), m_buffer->blockSize());
            
            if (bytesRead > 0) {
                dataBlock->type = TransferBlock::DATA;
                dataBlock->validSize = bytesRead;
                
                // Update Hash
                hash.addData((const char*)dataBlock->buffer.data(), bytesRead);
                
                m_buffer->pushFilledBlock(dataBlock);
            } else {
                m_buffer->releaseBlock(dataBlock); // Should not happen unless error
                break;
            }
        }
        
        file.close();

        // 3. Send FILE_END
        if (!m_abort) {
            TransferBlock* endBlock = m_buffer->acquireEmptyBlock();
            endBlock->type = TransferBlock::FILE_END;
            endBlock->fileName = fileName;
            endBlock->checksum = QString(hash.result().toHex());
            m_buffer->pushFilledBlock(endBlock);
        }
    }

    m_buffer->setFinished();
}

void TransferEngine::writerLoop()
{
    // Check if we are writing to tape or file
    bool toTape = !m_destDevice.isEmpty() && m_deviceManager != nullptr;
    
    QFile destFile;
    if (toTape) {
        if (!m_deviceManager->openDevice(m_destDevice)) {
            emit errorOccurred("Failed to open tape device: " + m_destDevice);
            m_abort = true;
            // Don't return immediately, let the loop drain the buffer or handle abort
        }
    }

    QElapsedTimer timer;
    timer.start();
    quint64 bytesSinceLastReport = 0;
    qint64 lastReportTime = 0;

    while (true) {
        TransferBlock* block = m_buffer->acquireFilledBlock();
        if (!block) break; // Finished

        if (m_abort) {
            m_buffer->releaseBlock(block);
            continue;
        }

        if (block->type == TransferBlock::FILE_START) {
            if (toTape) {
                // For raw tape write, we just log. 
                // In a real app, we might write a header block here.
                emit fileStarted(block->fileName);
            } else {
                QString destPath = QDir(m_destPath).filePath(block->fileName);
                destFile.setFileName(destPath);
                if (!destFile.open(QIODevice::WriteOnly)) {
                    emit errorOccurred("Failed to create destination file: " + destPath);
                    m_abort = true;
                } else {
                    emit fileStarted(block->fileName);
                }
            }
        }
        else if (block->type == TransferBlock::DATA) {
            bool success = false;
            if (toTape) {
                if (m_deviceManager->isDeviceOpen()) {
                    // Write to tape
                    QByteArray data((const char*)block->buffer.data(), block->validSize);
                    success = m_deviceManager->writeScsiBlock(data);
                    if (!success) {
                         emit errorOccurred("Write error on tape device");
                         m_abort = true;
                    }
                }
            } else {
                if (destFile.isOpen()) {
                    qint64 written = destFile.write((const char*)block->buffer.data(), block->validSize);
                    if (written == block->validSize) success = true;
                    else {
                        emit errorOccurred("Write error on file: " + destFile.fileName());
                        m_abort = true;
                    }
                }
            }
            
            if (success) {
                m_processedBytes += block->validSize;
                bytesSinceLastReport += block->validSize;
            }
        }
        else if (block->type == TransferBlock::FILE_END) {
            if (toTape) {
                if (m_deviceManager->isDeviceOpen()) {
                    // Write Filemark
                    if (!m_deviceManager->writeFileMark(1)) {
                        emit errorOccurred("Failed to write filemark");
                        m_abort = true;
                    }
                    emit fileFinished(block->fileName, block->checksum);
                }
            } else {
                if (destFile.isOpen()) {
                    destFile.close();
                    emit fileFinished(block->fileName, block->checksum);
                }
            }
        }

        m_buffer->releaseBlock(block);

        // Report Progress every 500ms
        if (timer.elapsed() - lastReportTime > 500) {
            double speed = (double)bytesSinceLastReport / (timer.elapsed() - lastReportTime) * 1000.0 / (1024.0 * 1024.0);
            emit progress(m_processedBytes, m_totalBytes, speed);
            
            lastReportTime = timer.elapsed();
            bytesSinceLastReport = 0;
        }
    }
    
    if (toTape) {
        m_deviceManager->closeDevice();
    } else {
        if (destFile.isOpen()) destFile.close();
    }
    
    m_running = false;
    emit finished();
}
