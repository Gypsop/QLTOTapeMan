#include "IndexManager.h"
#include "../utils/SettingsManager.h"
#include "LtfsIndexParser.h"
#include <QFile>
#include <QDir>
#include <QDebug>
#include <utility>

IndexManager::IndexManager(QObject *parent) : QObject(parent)
{
}

QString IndexManager::getIndexFilePath(const QString &volumeUuid) const
{
    QString dir = SettingsManager::instance().indexStoragePath();
    return dir + "/" + volumeUuid + ".schema";
}

QString IndexManager::saveIndex(const QString &volumeUuid, const QByteArray &xmlContent)
{
    QString path = getIndexFilePath(volumeUuid);
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(xmlContent);
        file.close();
        return path;
    }
    return QString();
}

bool IndexManager::loadIndex(const QString &volumeUuid, LtfsIndex &index)
{
    QString path = getIndexFilePath(volumeUuid);
    LtfsIndexParser parser;
    return parser.parse(path, index);
}

bool IndexManager::hasIndex(const QString &volumeUuid) const
{
    return QFile::exists(getIndexFilePath(volumeUuid));
}

QStringList IndexManager::listAvailableIndexes() const
{
    QString dirPath = SettingsManager::instance().indexStoragePath();
    QDir dir(dirPath);
    QStringList filters;
    filters << "*.schema";
    QStringList files = dir.entryList(filters, QDir::Files);
    
    QStringList uuids;
    for (const QString &file : std::as_const(files)) {
        uuids << QFileInfo(file).baseName();
    }
    return uuids;
}
