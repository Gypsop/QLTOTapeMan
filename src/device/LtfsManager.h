#ifndef LTFSMANAGER_H
#define LTFSMANAGER_H

#include <QObject>
#include <QString>
#include <QProcess>

class LtfsManager : public QObject
{
    Q_OBJECT
public:
    explicit LtfsManager(QObject *parent = nullptr);

    // Mount LTFS
    // devicePath: e.g., "0" (Tape 0) on Windows, "/dev/st0" on Linux
    // mountPoint: e.g., "X:" on Windows, "/mnt/ltfs" on Linux
    void mount(const QString &devicePath, const QString &mountPoint);

    // Unmount LTFS
    // mountPoint: The path to unmount
    void unmount(const QString &mountPoint);

    // Format tape (mkltfs)
    void format(const QString &devicePath, const QString &volumeName);

    // Check/Recover tape (ltfsck)
    void check(const QString &devicePath, bool deepRecovery = false);

signals:
    void operationStarted(const QString &operation);
    void operationFinished(const QString &operation, bool success, const QString &message);
    void outputReceived(const QString &text);

private:
    void runProcess(const QString &program, const QStringList &arguments, const QString &opName);
};

#endif // LTFSMANAGER_H
