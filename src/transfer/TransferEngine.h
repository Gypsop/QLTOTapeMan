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
    void setSourceDevice(const QString& devicePath); // For tape-to-file
    void setDestinationPath(const QString& path); // Legacy, for file-to-file
    void setDestinationDevice(const QString& devicePath); // For tape
    void setDeviceManager(DeviceManager* deviceManager);
    void setRewindSourceBefore(bool enable) { m_rewindSourceBefore = enable; }
    void setRewindDestBefore(bool enable) { m_rewindDestBefore = enable; }
    void setTapeDumpPrefix(const QString& prefix) { m_tapeDumpPrefix = prefix; }
    
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
    QString m_sourceDevice; // New: Source device path
    QString m_destPath;
    QString m_destDevice;
    DeviceManager* m_deviceManager = nullptr;

    int m_sourceHandleId = -1;
    int m_destHandleId = -1;
    
    RingBuffer* m_buffer = nullptr;
    
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_abort{false};
    
    std::thread m_readerThread;
    std::thread m_writerThread;
    
    quint64 m_totalBytes = 0;
    quint64 m_processedBytes = 0;

    bool m_rewindSourceBefore = true;
    bool m_rewindDestBefore = true;
    QString m_tapeDumpPrefix = "tape_dump";
};
