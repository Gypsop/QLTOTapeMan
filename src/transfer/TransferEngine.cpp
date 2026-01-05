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
    m_sourceDevice.clear(); // Clear device source if files are set
    m_totalBytes = 0;
    for (const QString& f : files) {
        m_totalBytes += QFileInfo(f).size();
    }
}

void TransferEngine::setSourceDevice(const QString& devicePath)
{
    m_sourceDevice = devicePath;
    m_sourceFiles.clear(); // Clear file source if device is set
    // Total bytes unknown for raw tape read usually, or we can estimate
    m_totalBytes = 0; 
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
    // Check if reading from tape
    if (!m_sourceDevice.isEmpty() && m_deviceManager) {
        if (!m_deviceManager->openDevice(m_sourceDevice)) {
            emit errorOccurred("Failed to open source tape device: " + m_sourceDevice);
            m_buffer->setFinished();
            return;
        }
        
        // Raw Tape Read Logic
        // We treat the tape content as a sequence of files separated by Filemarks
        
        int fileIndex = 0;
        
        while (!m_abort) {
            QString fileName = QString("tape_dump_%1.bin").arg(fileIndex, 3, 10, QChar('0'));
            
            // 1. Send FILE_START
            TransferBlock* startBlock = m_buffer->acquireEmptyBlock();
            startBlock->type = TransferBlock::FILE_START;
            startBlock->fileName = fileName;
            startBlock->fileSize = 0; // Unknown
            m_buffer->pushFilledBlock(startBlock);
            
            QCryptographicHash hash(QCryptographicHash::Sha1);
            bool fileMarkEncountered = false;
            
            // Read loop for current file
            while (!m_abort) {
                TransferBlock* dataBlock = m_buffer->acquireEmptyBlock();
                
                // Read from tape
                DeviceManager::ScsiReadResult result = m_deviceManager->readScsiBlock(m_buffer->blockSize());
                
                if (result.isError) {
                    m_buffer->releaseBlock(dataBlock);
                    emit errorOccurred("Tape Read Error: " + result.errorMessage);
                    m_abort = true;
                    break;
                }
                
                if (result.isFileMark) {
                    m_buffer->releaseBlock(dataBlock);
                    fileMarkEncountered = true;
                    break; // End of this file
                }
                
                if (result.isEOD || result.isEOM) {
                    m_buffer->releaseBlock(dataBlock);
                    m_abort = true; // End of all data
                    break;
                }
                
                if (result.data.isEmpty()) {
                    // Should not happen if no error/FM/EOD, but just in case
                    m_buffer->releaseBlock(dataBlock);
                    break;
                }
                
                // Copy data to buffer
                memcpy(dataBlock->buffer.data(), result.data.constData(), result.data.size());
                dataBlock->type = TransferBlock::DATA;
                dataBlock->validSize = result.data.size();
                
                hash.addData(QByteArrayView(dataBlock->buffer.data(), result.data.size()));
                m_buffer->pushFilledBlock(dataBlock);
            }
            
            // 3. Send FILE_END
            TransferBlock* endBlock = m_buffer->acquireEmptyBlock();
            endBlock->type = TransferBlock::FILE_END;
            endBlock->fileName = fileName;
            endBlock->checksum = QString(hash.result().toHex());
            m_buffer->pushFilledBlock(endBlock);
            
            if (!fileMarkEncountered || m_abort) {
                break;
            }
            
            fileIndex++;
        }
        
        m_buffer->setFinished();
        return;
    }

    // File Reader Logic
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
                hash.addData(QByteArrayView(dataBlock->buffer.data(), bytesRead));
                
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
                    DeviceManager::ScsiWriteResult result = m_deviceManager->writeScsiBlock(data);
                    success = !result.isError;
                    
                    if (result.isEOM) {
                        emit errorOccurred("End of Media reached during write");
                        m_abort = true;
                        success = false;
                    } else if (result.isError) {
                         emit errorOccurred("Write error on tape device: " + result.errorMessage);
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
        if (m_deviceManager->isDeviceOpen()) {
            m_deviceManager->synchronizeCache();
        }
        m_deviceManager->closeDevice();
    } else {
        if (destFile.isOpen()) destFile.close();
    }
    
    m_running = false;
    emit finished();
}
