#include "SettingsManager.h"
#include <QStandardPaths>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDebug>

SettingsManager& SettingsManager::instance()
{
    static SettingsManager instance;
    return instance;
}

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
    , m_settings("QLTOTapeMan", "Settings")
{
    // Ensure index storage path exists
    QDir dir(indexStoragePath());
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // Auto-detect if not set
    if (ltfsBinaryPath().isEmpty()) {
        autoDetectLtfsPaths();
    }
}

QString SettingsManager::ltfsBinaryPath() const
{
    return m_settings.value("ltfsBinaryPath").toString();
}

void SettingsManager::setLtfsBinaryPath(const QString &path)
{
    m_settings.setValue("ltfsBinaryPath", path);
}

QString SettingsManager::mkltfsBinaryPath() const
{
    return m_settings.value("mkltfsBinaryPath").toString();
}

void SettingsManager::setMkltfsBinaryPath(const QString &path)
{
    m_settings.setValue("mkltfsBinaryPath", path);
}

QString SettingsManager::ltfsckBinaryPath() const
{
    return m_settings.value("ltfsckBinaryPath").toString();
}

void SettingsManager::setLtfsckBinaryPath(const QString &path)
{
    m_settings.setValue("ltfsckBinaryPath", path);
}

QString SettingsManager::indexStoragePath() const
{
    // Store indexes in the application directory for portability, or UserData
    // User requested "program's directory".
    QString path = QCoreApplication::applicationDirPath() + "/indexes";
    return path;
}

void SettingsManager::autoDetectLtfsPaths()
{
    QStringList searchPaths;
    QString ltfsName = "ltfs";
    QString mkltfsName = "mkltfs";
    QString ltfsckName = "ltfsck";

#ifdef Q_OS_WIN
    ltfsName += ".exe";
    mkltfsName += ".exe";
    ltfsckName += ".exe";
    searchPaths << "C:/Program Files/HPE/LTFS"
                << "C:/Program Files/IBM/LTFS"
                << "C:/Program Files/Quantum/LTFS"
                << "C:/Program Files/Dell/LTFS";
#else
    searchPaths << "/usr/bin"
                << "/usr/local/bin"
                << "/opt/ibm/ltfs/bin"
                << "/opt/hpe/ltfs/bin"
                << "/opt/local/bin"; // MacPorts
#endif

    // Search for ltfs
    for (const QString &path : searchPaths) {
        QString fullPath = path + "/" + ltfsName;
        if (QFileInfo::exists(fullPath)) {
            setLtfsBinaryPath(fullPath);
            break;
        }
    }

    // Search for mkltfs
    for (const QString &path : searchPaths) {
        QString fullPath = path + "/" + mkltfsName;
        if (QFileInfo::exists(fullPath)) {
            setMkltfsBinaryPath(fullPath);
            break;
        }
    }
    
    // Search for ltfsck
    for (const QString &path : searchPaths) {
        QString fullPath = path + "/" + ltfsckName;
        if (QFileInfo::exists(fullPath)) {
            setLtfsckBinaryPath(fullPath);
            break;
        }
    }
    
    // Fallback: Check system PATH
    if (ltfsBinaryPath().isEmpty()) {
        QString path = QStandardPaths::findExecutable(ltfsName);
        if (!path.isEmpty()) setLtfsBinaryPath(path);
    }
    if (mkltfsBinaryPath().isEmpty()) {
        QString path = QStandardPaths::findExecutable(mkltfsName);
        if (!path.isEmpty()) setMkltfsBinaryPath(path);
    }
    if (ltfsckBinaryPath().isEmpty()) {
        QString path = QStandardPaths::findExecutable(ltfsckName);
        if (!path.isEmpty()) setLtfsckBinaryPath(path);
    }
}
