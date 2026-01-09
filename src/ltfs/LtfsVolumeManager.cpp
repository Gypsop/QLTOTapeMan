#include "LtfsVolumeManager.h"
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>
#include <QXmlStreamWriter>
#include <functional>

LtfsVolumeManager::LtfsVolumeManager(QObject *parent) : QObject(parent)
{
}

bool LtfsVolumeManager::writeVolume(DeviceManager *dm,
                                    const QString &devicePath,
                                    const QStringList &sourceFiles,
                                    const QString &volumeUuid,
                                    QString &errorMessage)
{
    if (!dm) { errorMessage = "DeviceManager is null"; return false; }
    if (!dm->openDevice(devicePath)) { errorMessage = "Failed to open device"; return false; }

    // Fixed block size for deterministic extents
    if (!dm->setBlockSize(kBlockSize)) {
        errorMessage = "Failed to set block size";
        dm->closeDevice();
        return false;
    }

    // Rewind, then switch to data partition (1)
    dm->rewindDevice(devicePath);
    if (!dm->locate(0, 1)) {
        errorMessage = "Failed to locate data partition";
        dm->closeDevice();
        return false;
    }

    LtfsIndex index;
    index.volumeUuid = volumeUuid;
    index.creator = "QLTOTapeMan";
    index.generationNumber = 1;
    index.updateTime = QDateTime::currentDateTimeUtc();
    index.locationPartition = "a";
    index.locationStartBlock = 0;
    index.volumeLockState = "unlocked";
    index.highestFileUid = 1;

    if (!writeDataPartition(dm, sourceFiles, index, errorMessage)) {
        dm->closeDevice();
        return false;
    }

    // After data written, rewind and write index to partition 0
    dm->rewindDevice(devicePath);
    if (!dm->locate(0, 0)) {
        errorMessage = "Failed to locate index partition";
        dm->closeDevice();
        return false;
    }

    if (!writeIndexPartition(dm, index, errorMessage)) {
        dm->closeDevice();
        return false;
    }

    dm->synchronizeCache();
    dm->closeDevice();
    return true;
}

bool LtfsVolumeManager::writeDataPartition(DeviceManager *dm,
                                           const QStringList &sourceFiles,
                                           LtfsIndex &index,
                                           QString &errorMessage)
{
    quint64 currentBlock = 0;
    for (const QString &path : sourceFiles) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            errorMessage = QString("Cannot open %1").arg(path);
            return false;
        }
        QFileInfo info(f);
        LtfsFile lf;
        lf.name = info.fileName();
        lf.length = info.size();
        lf.readonly = false;
        lf.creationTime = info.birthTime().isValid() ? info.birthTime() : QDateTime::currentDateTimeUtc();
        lf.modifyTime = info.lastModified();
        lf.changeTime = lf.modifyTime;
        lf.accessTime = info.lastRead();
        lf.fileUid = index.highestFileUid++;

        QCryptographicHash hash(QCryptographicHash::Sha1);
        quint64 fileOffset = 0;
        while (!f.atEnd()) {
            QByteArray chunk = f.read(kBlockSize);
            if (chunk.isEmpty() && f.error() != QFile::NoError) {
                errorMessage = QString("Read error on %1").arg(path);
                return false;
            }
            hash.addData(chunk);

            DeviceManager::ScsiWriteResult wr = dm->writeScsiBlock(chunk);
            if (wr.isError) {
                errorMessage = QString("Tape write failed: %1").arg(wr.errorMessage);
                return false;
            }

            LtfsExtent ext;
            ext.startBlock = currentBlock;
            ext.byteOffset = fileOffset;
            ext.byteCount = static_cast<quint64>(chunk.size());
            ext.partition = "b";
            ext.fileOffset = fileOffset;
            lf.extents.append(ext);

            currentBlock += 1;
            fileOffset += chunk.size();
        }

        lf.sha1 = QString(hash.result().toHex());

        index.rootDirectory.files.append(lf);

        // filemark between files
        if (!dm->writeFileMark(1)) {
            errorMessage = "Failed to write filemark";
            return false;
        }
        currentBlock += 1; // filemark consumes a logical position
    }
    return true;
}

bool LtfsVolumeManager::writeIndexPartition(DeviceManager *dm,
                                            const LtfsIndex &index,
                                            QString &errorMessage)
{
    QByteArray xml = buildIndexXml(index);
    DeviceManager::ScsiWriteResult wr = dm->writeScsiBlock(xml);
    if (wr.isError) {
        errorMessage = QString("Index write failed: %1").arg(wr.errorMessage);
        return false;
    }
    if (!dm->writeFileMark(1)) {
        errorMessage = "Failed to write index filemark";
        return false;
    }
    return true;
}

QByteArray LtfsVolumeManager::buildIndexXml(const LtfsIndex &index)
{
    QByteArray xml;
    QXmlStreamWriter w(&xml);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    w.writeStartElement("ltfsindex");
    w.writeAttribute("volumeuuid", index.volumeUuid);
    w.writeAttribute("creator", index.creator);
    w.writeAttribute("generationnumber", QString::number(index.generationNumber));
    w.writeAttribute("updatetime", index.updateTime.toString(Qt::ISODate));

    std::function<void(const LtfsDirectory&)> writeDir = [&](const LtfsDirectory &dir){
        w.writeStartElement("directory");
        w.writeAttribute("name", dir.name.isEmpty() ? "/" : dir.name);
        for (const LtfsFile &f : dir.files) {
            w.writeStartElement("file");
            w.writeAttribute("name", f.name);
            w.writeAttribute("length", QString::number(f.length));
            w.writeAttribute("readonly", f.readonly ? "true" : "false");
            w.writeAttribute("sha1", f.sha1);
            if (!f.sha1.isEmpty()) {
                w.writeStartElement("xattr");
                w.writeTextElement("key", "user.ltfs.hash.sha1");
                w.writeTextElement("value", f.sha1);
                w.writeEndElement();
            }
            for (const LtfsExtent &e : f.extents) {
                w.writeStartElement("extent");
                w.writeAttribute("startblock", QString::number(e.startBlock));
                w.writeAttribute("byteoffset", QString::number(e.byteOffset));
                w.writeAttribute("bytecount", QString::number(e.byteCount));
                w.writeAttribute("partition", e.partition);
                w.writeAttribute("fileoffset", QString::number(e.fileOffset));
                w.writeEndElement();
            }
            w.writeEndElement(); // file
        }
        for (const LtfsDirectory &sd : dir.subdirectories) {
            writeDir(sd);
        }
        w.writeEndElement(); // directory
    };

    writeDir(index.rootDirectory);
    w.writeEndElement(); // ltfsindex
    w.writeEndDocument();
    return xml;
}

bool LtfsVolumeManager::readIndex(DeviceManager *dm,
                                  LtfsIndex &index,
                                  QString &errorMessage)
{
    if (!dm->locate(0, 0)) { errorMessage = "Locate index partition failed"; return false; }
    DeviceManager::ScsiReadResult rd = dm->readScsiBlock(kBlockSize);
    if (rd.isError) { errorMessage = rd.errorMessage; return false; }
    if (rd.data.isEmpty()) { errorMessage = "Empty index data"; return false; }

    LtfsIndexParser parser;
    QString tempPath = QDir::temp().filePath("ltfs_index_tmp.xml");
    QFile tf(tempPath);
    if (!tf.open(QIODevice::WriteOnly)) { errorMessage = "Cannot write temp index"; return false; }
    tf.write(rd.data);
    tf.close();

    if (!parser.parse(tempPath, index)) {
        errorMessage = parser.errorString();
        QFile::remove(tempPath);
        return false;
    }
    QFile::remove(tempPath);
    return true;
}

bool LtfsVolumeManager::readFileExtent(DeviceManager *dm,
                                       const LtfsExtent &ext,
                                       QByteArray &outData,
                                       QString &errorMessage)
{
    uint32_t partition = (ext.partition.toLower() == "a") ? 0 : 1;
    if (!dm->locate(ext.startBlock, partition)) {
        errorMessage = "Locate extent failed";
        return false;
    }

    quint64 remaining = ext.byteCount;
    QByteArray result;
    while (remaining > 0) {
        uint32_t chunk = static_cast<uint32_t>(qMin<quint64>(remaining, kBlockSize));
        auto rd = dm->readScsiBlock(chunk);
        if (rd.isError) { errorMessage = rd.errorMessage; return false; }
        result.append(rd.data);
        remaining -= rd.data.size();
        if (rd.data.isEmpty()) break;
    }
    if (static_cast<quint64>(result.size()) < ext.byteCount) {
        errorMessage = "Unexpected short read";
        return false;
    }
    outData = result.left(ext.byteCount);
    return true;
}

bool LtfsVolumeManager::verifyAndExtract(DeviceManager *dm,
                                         const QString &devicePath,
                                         const QString &destDir,
                                         QString &errorMessage)
{
    if (!dm) { errorMessage = "DeviceManager is null"; return false; }
    if (!dm->openDevice(devicePath)) { errorMessage = "Failed to open device"; return false; }
    if (!dm->setBlockSize(kBlockSize)) {
        errorMessage = "Failed to set block size";
        dm->closeDevice();
        return false;
    }

    LtfsIndex index;
    if (!readIndex(dm, index, errorMessage)) {
        dm->closeDevice();
        return false;
    }

    for (const LtfsFile &f : index.rootDirectory.files) {
        QByteArray data;
        if (f.extents.isEmpty()) { errorMessage = "Missing extent"; dm->closeDevice(); return false; }
        for (const LtfsExtent &ext : f.extents) {
            QByteArray part;
            if (!readFileExtent(dm, ext, part, errorMessage)) {
                dm->closeDevice();
                return false;
            }
            data.append(part);
        }
        QCryptographicHash hash(QCryptographicHash::Sha1);
        hash.addData(data);
        QString h = QString(hash.result().toHex());
        if (!f.sha1.isEmpty() && f.sha1.compare(h, Qt::CaseInsensitive) != 0) {
            errorMessage = QString("Hash mismatch for %1").arg(f.name);
            dm->closeDevice();
            return false;
        }
        if (!destDir.isEmpty()) {
            QDir().mkpath(destDir);
            QString outPath = QDir(destDir).filePath(f.name);
            QFile out(outPath);
            if (!out.open(QIODevice::WriteOnly)) {
                errorMessage = QString("Cannot open dest %1").arg(outPath);
                dm->closeDevice();
                return false;
            }
            out.write(data);
            out.close();
        }
    }

    dm->closeDevice();
    return true;
}
