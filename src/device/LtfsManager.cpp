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

void LtfsManager::format(const QString &devicePath, const QString &volumeName)
{
    QString program = SettingsManager::instance().mkltfsBinaryPath();
    if (program.isEmpty()) program = "mkltfs";
    
    QStringList args;
    
#ifdef Q_OS_WIN
    args << "-d" << devicePath << "-n" << volumeName;
#else
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
