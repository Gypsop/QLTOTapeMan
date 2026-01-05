#include "LtfsManager.h"
#include <QDebug>
#include <QDir>

LtfsManager::LtfsManager(QObject *parent) : QObject(parent)
{
}

void LtfsManager::mount(const QString &devicePath, const QString &mountPoint)
{
    QString program;
    QStringList args;

#ifdef Q_OS_WIN
    program = "ltfs.exe";
    // Windows syntax: ltfs <drive_letter> -o devname=<device_id>
    // Assuming devicePath is the device index or ID.
    args << mountPoint << "-o" << QString("devname=%1").arg(devicePath);
#else
    program = "ltfs";
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
    // Windows: Usually just stop the service or kill the process, but standard way might be different.
    // Often unmount is done by ejecting or specific tool.
    // For now, we might need to kill the ltfs process associated with the drive, or use a specific unmount command if provided by the driver suite.
    // A generic way is difficult on Windows without the specific driver API.
    // Placeholder:
    program = "taskkill"; 
    args << "/F" << "/IM" << "ltfs.exe"; // Very rough, kills all LTFS instances!
#elif defined(Q_OS_MAC)
    program = "umount";
    args << mountPoint;
#else
    program = "umount";
    args << mountPoint;
#endif

    runProcess(program, args, "Unmount");
}

void LtfsManager::format(const QString &devicePath, const QString &volumeName)
{
    QString program;
    QStringList args;

#ifdef Q_OS_WIN
    program = "mkltfs.exe";
    args << "-d" << devicePath << "-n" << volumeName;
#else
    program = "mkltfs";
    args << "-d" << devicePath << "-n" << volumeName;
#endif

    runProcess(program, args, "Format");
}

void LtfsManager::runProcess(const QString &program, const QStringList &arguments, const QString &opName)
{
    QProcess *process = new QProcess(this);
    
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        emit outputReceived(process->readAllStandardOutput());
    });
    
    connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
        emit outputReceived(process->readAllStandardError());
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process, opName](int exitCode, QProcess::ExitStatus exitStatus) {
        bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);
        emit operationFinished(opName, success, success ? "Success" : "Failed");
        process->deleteLater();
    });

    emit operationStarted(opName);
    process->start(program, arguments);
}
