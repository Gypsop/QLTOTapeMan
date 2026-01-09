#pragma once

#include <QObject>
#include <QStringList>
#include <QByteArray>
#include <QCryptographicHash>
#include "LtfsElements.h"
#include "LtfsIndexParser.h"
#include "../device/DeviceManager.h"

class LtfsVolumeManager : public QObject {
    Q_OBJECT
public:
    explicit LtfsVolumeManager(QObject *parent = nullptr);

    // Write a simple LTFS volume: data in partition 1 ("b"), index in partition 0 ("a").
    bool writeVolume(DeviceManager *dm,
                     const QString &devicePath,
                     const QStringList &sourceFiles,
                     const QString &volumeUuid,
                     QString &errorMessage);

    // Verify and optionally extract files to destDir (if non-empty). Returns true on success.
    bool verifyAndExtract(DeviceManager *dm,
                          const QString &devicePath,
                          const QString &destDir,
                          QString &errorMessage);

private:
    static constexpr uint32_t kBlockSize = 1024 * 1024; // 1MB blocks

    bool writeDataPartition(DeviceManager *dm,
                            const QStringList &sourceFiles,
                            LtfsIndex &index,
                            QString &errorMessage);

    bool writeIndexPartition(DeviceManager *dm,
                             const LtfsIndex &index,
                             QString &errorMessage);

    QByteArray buildIndexXml(const LtfsIndex &index);

    bool readIndex(DeviceManager *dm,
                   LtfsIndex &index,
                   QString &errorMessage);

    bool readFileExtent(DeviceManager *dm,
                        const LtfsExtent &ext,
                        QByteArray &outData,
                        QString &errorMessage);
};
