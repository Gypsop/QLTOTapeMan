#pragma once

#include <QObject>
#include <QStringList>
#include <thread>
#include <atomic>
#include "RingBuffer.h"
#include "../device/DeviceManager.h"

class TransferEngine : public QObject {
    Q_OBJECT
public:
    explicit TransferEngine(QObject *parent = nullptr);
    ~TransferEngine();

    // Configure the transfer
    void setSourceFiles(const QStringList& files);
    void setDestinationPath(const QString& path); // Legacy, for file-to-file
    void setDestinationDevice(const QString& devicePath); // For tape
    void setDeviceManager(DeviceManager* deviceManager);
    
    // Start the transfer process
    void start();
    
    // Stop/Abort the transfer
    void stop();
    
    bool isRunning() const { return m_running; }

signals:
    // Emitted when a file starts transferring
    void fileStarted(QString fileName);
    
    // Emitted when a file is successfully written
    void fileFinished(QString fileName, QString checksum);
    
    // Progress update (total for the current session)
    void progress(quint64 bytesTransferred, quint64 totalBytes, double speedMBps);
    
    // Emitted when all files are done
    void finished();
    
    // Emitted on error
    void errorOccurred(QString error);

private:
    void readerLoop();
    void writerLoop();

    QStringList m_sourceFiles;
    QString m_destPath;
    QString m_destDevice;
    DeviceManager* m_deviceManager = nullptr;
    
    RingBuffer* m_buffer = nullptr;
    
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_abort{false};
    
    std::thread m_readerThread;
    std::thread m_writerThread;
    
    quint64 m_totalBytes = 0;
    quint64 m_processedBytes = 0;
};
