#include "LtfsManager.h"
#include "../utils/SettingsManager.h"
#include <QDebug>
#include <QDir>

LtfsManager::LtfsManager(QObject *parent) : QObject(parent)
{
}

void LtfsManager::mount(const QString &devicePath, const QString &mountPoint)
{
    QString program = SettingsManager::instance().ltfsBinaryPath();
    if (program.isEmpty()) program = "ltfs";
    
    QStringList args;

#ifdef Q_OS_WIN
    // Windows syntax: ltfs <drive_letter> -o devname=<device_id>
    args << mountPoint << "-o" << QString("devname=%1").arg(devicePath);
#else
    // Linux/Mac syntax: ltfs -o devname=<device_path> <mount_point>
    args << "-o" << QString("devname=%1").arg(devicePath) << mountPoint;
#endif

    runProcess(program, args, "Mount");
}

void LtfsManager::unmount(const QString &mountPoint)
{
    QString program;
    QStringList args;

#ifdef Q_OS_WIN
    // Windows: Rough unmount
    program = "taskkill"; 
    args << "/F" << "/IM" << "ltfs.exe"; 
#else
    program = "umount";
    args << mountPoint;
#endif

    runProcess(program, args, "Unmount");
}

void LtfsManager::format(const QString &devicePath, const LtfsFormatOptions &options)
{
    QString program = SettingsManager::instance().mkltfsBinaryPath();
    if (program.isEmpty()) program = "mkltfs";
    
    QStringList args;
    
    // Device
    args << "-d" << devicePath;
    
    // Volume Name
    if (!options.volumeName.isEmpty()) {
        args << "-n" << options.volumeName;
    }
    
    // Tape Serial
    if (!options.tapeSerial.isEmpty()) {
        args << "-s" << options.tapeSerial;
    }
    
    // Block Size (default is usually fine, but user can override)
    // mkltfs usually takes -b in bytes? or KB?
    // Man page says: -b, --blocksize=SIZE. SIZE is in bytes.
    if (options.blockSize > 0) {
        args << "-b" << QString::number(options.blockSize);
    }
    
    // Compression
    if (options.compression) {
        args << "--compression";
    } else {
        args << "--no-compression";
    }
    
    // Wipe
    if (options.wipe) {
        args << "--wipe";
    }
    
    // Force
    if (options.force) {
        args << "--force";
    }

    // Index Partition Size
    if (options.indexPartitionSize > 0) {
        args << "--index-partition-size" << QString::number(options.indexPartitionSize);
    }

    // Encryption Key File
    if (!options.keyFile.isEmpty()) {
        args << "-k" << options.keyFile;
    }

    runProcess(program, args, "Format");
}

void LtfsManager::check(const QString &devicePath, const LtfsCheckOptions &options)
{
    QString program = SettingsManager::instance().ltfsckBinaryPath();
    if (program.isEmpty()) program = "ltfsck";
    
    QStringList args;
    
    if (options.deepRecovery) {
        args << "--deep-recovery";
    }
    
    if (options.fullRecovery) {
        args << "--full-recovery";
    }
    
    if (options.captureIndex) {
        // This usually requires a file path, but for now let's assume it just dumps to stdout or default
        // Actually -g takes a path. We might need to add that to options if we want to support it fully.
        // For now, let's skip or implement if requested.
    }
    
    args << devicePath;

    runProcess(program, args, "Check/Recover");
}

void LtfsManager::cancelOperation()
{
    if (m_currentProcess && m_currentProcess->state() != QProcess::NotRunning) {
        m_currentProcess->kill();
        m_currentProcess->waitForFinished(1000);
        emit outputReceived("Operation cancelled by user.\n");
    }
}

void LtfsManager::runProcess(const QString &program, const QStringList &arguments, const QString &opName)
{
    if (m_currentProcess && m_currentProcess->state() != QProcess::NotRunning) {
        emit outputReceived("Another operation is already running.\n");
        return;
    }

    m_currentProcess = new QProcess(this);
    
    connect(m_currentProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        emit outputReceived(m_currentProcess->readAllStandardOutput());
    });
    
    connect(m_currentProcess, &QProcess::readyReadStandardError, this, [this]() {
        emit outputReceived(m_currentProcess->readAllStandardError());
    });

    connect(m_currentProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, opName](int exitCode, QProcess::ExitStatus exitStatus) {
        bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);
        emit operationFinished(opName, success, success ? "Success" : "Failed");
        m_currentProcess->deleteLater();
        m_currentProcess = nullptr;
    });

    emit operationStarted(opName);
    m_currentProcess->start(program, arguments);
}
