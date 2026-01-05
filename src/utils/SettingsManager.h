#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QDir>

class SettingsManager : public QObject
{
    Q_OBJECT
public:
    static SettingsManager& instance();

    QString ltfsBinaryPath() const;
    void setLtfsBinaryPath(const QString &path);

    QString mkltfsBinaryPath() const;
    void setMkltfsBinaryPath(const QString &path);

    QString indexStoragePath() const;
    
    // Attempt to auto-detect LTFS binaries
    void autoDetectLtfsPaths();

private:
    explicit SettingsManager(QObject *parent = nullptr);
    QSettings m_settings;
};

#endif // SETTINGSMANAGER_H
