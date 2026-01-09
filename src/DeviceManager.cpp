#include "DeviceManager.h"
#include "TapeModel.h"

#include <QFileInfo>

DeviceManager::DeviceManager(QObject *parent)
    : QObject(parent)
{
}

QList<TapeDeviceInfo> DeviceManager::scanDevices()
{
    // TODO: Implement real device discovery; placeholder empty list
    return QList<TapeDeviceInfo>();
}

TapeNode DeviceManager::readTapeIndex(const QString &devicePath)
{
    Q_UNUSED(devicePath);
    // NOTE: Interpret devicePath as a mounted LTFS root directory for now.
    // If path is empty or unavailable, return sample tree.
    QFileInfo info(devicePath);
    if (!devicePath.isEmpty() && info.exists() && info.isDir()) {
        return TapeModel::fromDirectory(devicePath, QString("/"));
    }
    return TapeModel::makeSampleTree();
}
