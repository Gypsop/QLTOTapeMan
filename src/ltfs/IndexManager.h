#ifndef INDEXMANAGER_H
#define INDEXMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include "LtfsElements.h"

class IndexManager : public QObject
{
    Q_OBJECT
public:
    explicit IndexManager(QObject *parent = nullptr);
    
    // Save an index XML content to local storage
    // Returns the path where it was saved
    QString saveIndex(const QString &volumeUuid, const QByteArray &xmlContent);
    
    // Load an index from local storage
    bool loadIndex(const QString &volumeUuid, LtfsIndex &index);
    
    // Check if an index exists
    bool hasIndex(const QString &volumeUuid) const;

    // List all available local indexes (UUIDs)
    QStringList listAvailableIndexes() const;

private:
    QString getIndexFilePath(const QString &volumeUuid) const;
};

#endif // INDEXMANAGER_H
