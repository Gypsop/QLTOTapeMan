#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include "TapeModel.h"

struct TapeDeviceInfo {
    QString devicePath;
    QString displayName;
};

class DeviceManager : public QObject
{
    Q_OBJECT
public:
    explicit DeviceManager(QObject *parent = nullptr);

    QList<TapeDeviceInfo> scanDevices();
    TapeNode readTapeIndex(const QString &devicePath);
};

#endif // DEVICEMANAGER_H
