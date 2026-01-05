#ifndef LTFSMANAGER_H
#define LTFSMANAGER_H

#include <QObject>
#include <QString>
#include <QProcess>

struct LtfsFormatOptions {
    QString volumeName;
    QString tapeSerial;
    int blockSize = 524288; // 512KB
    bool compression = true;
    bool wipe = false;
    bool force = true;
    int indexPartitionSize = 0; // MB, 0 = default
    QString keyFile; // Path to key file for encryption
};

struct LtfsCheckOptions {
    bool deepRecovery = false;
    bool fullRecovery = false;
    bool captureIndex = false;
};

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
    void format(const QString &devicePath, const LtfsFormatOptions &options);

    // Check/Recover tape (ltfsck)
    void check(const QString &devicePath, const LtfsCheckOptions &options);

    // Cancel current operation
    void cancelOperation();

signals:
    void operationStarted(const QString &operation);
    void operationFinished(const QString &operation, bool success, const QString &message);
    void outputReceived(const QString &text);

private:
    void runProcess(const QString &program, const QStringList &arguments, const QString &opName);
    QProcess *m_currentProcess = nullptr;
};

#endif // LTFSMANAGER_H
