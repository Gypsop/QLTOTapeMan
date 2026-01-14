/*
 * QLTOTapeMan - Qt-based LTO Tape Manager
 * libqltfs - LTFS Core Library
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "IndexWriter.h"

#include <QFile>
#include <QDebug>

namespace qltfs {

// XML element names as per LTFS spec
static const QString ELEM_LTFSINDEX = QStringLiteral("ltfsindex");
static const QString ELEM_CREATOR = QStringLiteral("creator");
static const QString ELEM_VOLUME_UUID = QStringLiteral("volumeuuid");
static const QString ELEM_GENERATION = QStringLiteral("generationnumber");
static const QString ELEM_UPDATE_TIME = QStringLiteral("updatetime");
static const QString ELEM_LOCATION = QStringLiteral("location");
static const QString ELEM_PARTITION = QStringLiteral("partition");
static const QString ELEM_STARTBLOCK = QStringLiteral("startblock");
static const QString ELEM_PREVIOUS_GENERATION = QStringLiteral("previousgenerationlocation");
static const QString ELEM_ALLOW_POLICY_UPDATE = QStringLiteral("allowpolicyupdate");
static const QString ELEM_DATA_PLACEMENT_POLICY = QStringLiteral("dataplacementpolicy");
static const QString ELEM_INDEX_PLACEMENT_POLICY = QStringLiteral("indexplacementpolicy");
static const QString ELEM_HIGHEST_FILE_UID = QStringLiteral("highestfileuid");
static const QString ELEM_DIRECTORY = QStringLiteral("directory");
static const QString ELEM_FILE = QStringLiteral("file");
static const QString ELEM_NAME = QStringLiteral("name");
static const QString ELEM_READONLY = QStringLiteral("readonly");
static const QString ELEM_CREATION_TIME = QStringLiteral("creationtime");
static const QString ELEM_CHANGE_TIME = QStringLiteral("changetime");
static const QString ELEM_MODIFY_TIME = QStringLiteral("modifytime");
static const QString ELEM_ACCESS_TIME = QStringLiteral("accesstime");
static const QString ELEM_BACKUP_TIME = QStringLiteral("backuptime");
static const QString ELEM_LENGTH = QStringLiteral("length");
static const QString ELEM_EXTENT_INFO = QStringLiteral("extentinfo");
static const QString ELEM_EXTENT = QStringLiteral("extent");
static const QString ELEM_FILE_OFFSET = QStringLiteral("fileoffset");
static const QString ELEM_BYTE_OFFSET = QStringLiteral("byteoffset");
static const QString ELEM_BYTE_COUNT = QStringLiteral("bytecount");
static const QString ELEM_EXTENDED_ATTRIBUTE = QStringLiteral("extendedattribute");
static const QString ELEM_EA_KEY = QStringLiteral("key");
static const QString ELEM_EA_VALUE = QStringLiteral("value");
static const QString ELEM_UID = QStringLiteral("uid");

static const QString ATTR_VERSION = QStringLiteral("version");
static const QString LTFS_NAMESPACE = QStringLiteral("http://www.lto.org/ltfs");

// ============================================================================
// IndexWriter Private Implementation
// ============================================================================

class IndexWriter::Private
{
public:
    bool formatted = true;
    QString errorMessage;
};

// ============================================================================
// IndexWriter Implementation
// ============================================================================

IndexWriter::IndexWriter()
    : d(new Private)
{
}

IndexWriter::~IndexWriter()
{
    delete d;
}

void IndexWriter::setFormatted(bool formatted)
{
    d->formatted = formatted;
}

bool IndexWriter::isFormatted() const
{
    return d->formatted;
}

QByteArray IndexWriter::write(const LtfsIndex &index)
{
    QByteArray data;
    QXmlStreamWriter xml(&data);

    xml.setAutoFormatting(d->formatted);
    xml.setAutoFormattingIndent(2);

    xml.writeStartDocument(QStringLiteral("1.0"), true);

    writeIndex(xml, index);

    xml.writeEndDocument();

    return data;
}

QByteArray IndexWriter::write(const QSharedPointer<LtfsIndex> &index)
{
    if (!index) {
        d->errorMessage = QStringLiteral("Null index pointer");
        return QByteArray();
    }
    return write(*index);
}

bool IndexWriter::writeFile(const LtfsIndex &index, const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        d->errorMessage = QStringLiteral("Failed to open file: %1").arg(file.errorString());
        return false;
    }

    QByteArray data = write(index);
    if (data.isEmpty()) {
        return false;
    }

    if (file.write(data) != data.size()) {
        d->errorMessage = QStringLiteral("Failed to write file: %1").arg(file.errorString());
        return false;
    }

    return true;
}

bool IndexWriter::writeFile(const QSharedPointer<LtfsIndex> &index, const QString &filePath)
{
    if (!index) {
        d->errorMessage = QStringLiteral("Null index pointer");
        return false;
    }
    return writeFile(*index, filePath);
}

QString IndexWriter::errorMessage() const
{
    return d->errorMessage;
}

void IndexWriter::writeIndex(QXmlStreamWriter &xml, const LtfsIndex &index)
{
    xml.writeStartElement(ELEM_LTFSINDEX);
    xml.writeAttribute(ATTR_VERSION, index.version().isEmpty() ? QStringLiteral("2.4.0") : index.version());
    xml.writeDefaultNamespace(LTFS_NAMESPACE);

    // Creator
    xml.writeTextElement(ELEM_CREATOR, index.creator());

    // Volume UUID
    xml.writeTextElement(ELEM_VOLUME_UUID, index.volumeUuid().toString(QUuid::WithoutBraces));

    // Generation number
    xml.writeTextElement(ELEM_GENERATION, QString::number(index.generationNumber()));

    // Update time
    xml.writeTextElement(ELEM_UPDATE_TIME, formatTimestamp(index.updateTime()));

    // Self location
    writeLocation(xml, ELEM_LOCATION, index.selfLocation());

    // Previous generation location (optional)
    if (index.previousGenerationLocation().startBlock > 0) {
        writeLocation(xml, ELEM_PREVIOUS_GENERATION, index.previousGenerationLocation());
    }

    // Allow policy update
    xml.writeTextElement(ELEM_ALLOW_POLICY_UPDATE, formatBoolean(index.allowPolicyUpdate()));

    // Highest file UID
    xml.writeTextElement(ELEM_HIGHEST_FILE_UID, QString::number(index.highestFileUid()));

    // Root directory
    writeDirectory(xml, index.rootDirectory());

    xml.writeEndElement(); // ltfsindex
}

void IndexWriter::writeDirectory(QXmlStreamWriter &xml, const LtfsDirectory &directory)
{
    xml.writeStartElement(ELEM_DIRECTORY);

    // Name
    xml.writeTextElement(ELEM_NAME, directory.name());

    // UID (if set)
    if (directory.uid() > 0) {
        xml.writeTextElement(ELEM_UID, QString::number(directory.uid()));
    }

    // Readonly
    xml.writeTextElement(ELEM_READONLY, formatBoolean(directory.readonly()));

    // Timestamps
    xml.writeTextElement(ELEM_CREATION_TIME, formatTimestamp(directory.creationTime()));
    xml.writeTextElement(ELEM_CHANGE_TIME, formatTimestamp(directory.changeTime()));
    xml.writeTextElement(ELEM_MODIFY_TIME, formatTimestamp(directory.modifyTime()));
    xml.writeTextElement(ELEM_ACCESS_TIME, formatTimestamp(directory.accessTime()));

    if (directory.backupTime().isValid()) {
        xml.writeTextElement(ELEM_BACKUP_TIME, formatTimestamp(directory.backupTime()));
    }

    // Extended attributes
    for (const auto &attr : directory.extendedAttributes()) {
        writeExtendedAttribute(xml, attr);
    }

    // Files
    for (const auto &file : directory.files()) {
        writeFile(xml, file);
    }

    // Subdirectories
    for (const auto &subdir : directory.subdirectories()) {
        writeDirectory(xml, subdir);
    }

    xml.writeEndElement(); // directory
}

void IndexWriter::writeFile(QXmlStreamWriter &xml, const LtfsFile &file)
{
    xml.writeStartElement(ELEM_FILE);

    // Name
    xml.writeTextElement(ELEM_NAME, file.name());

    // UID (if set)
    if (file.uid() > 0) {
        xml.writeTextElement(ELEM_UID, QString::number(file.uid()));
    }

    // Length
    xml.writeTextElement(ELEM_LENGTH, QString::number(file.length()));

    // Readonly
    xml.writeTextElement(ELEM_READONLY, formatBoolean(file.readonly()));

    // Timestamps
    xml.writeTextElement(ELEM_CREATION_TIME, formatTimestamp(file.creationTime()));
    xml.writeTextElement(ELEM_CHANGE_TIME, formatTimestamp(file.changeTime()));
    xml.writeTextElement(ELEM_MODIFY_TIME, formatTimestamp(file.modifyTime()));
    xml.writeTextElement(ELEM_ACCESS_TIME, formatTimestamp(file.accessTime()));

    if (file.backupTime().isValid()) {
        xml.writeTextElement(ELEM_BACKUP_TIME, formatTimestamp(file.backupTime()));
    }

    // Extended attributes
    for (const auto &attr : file.extendedAttributes()) {
        writeExtendedAttribute(xml, attr);
    }

    // Extent info
    const auto &extents = file.extentInfo();
    if (!extents.isEmpty()) {
        xml.writeStartElement(ELEM_EXTENT_INFO);
        for (const auto &extent : extents) {
            writeExtent(xml, extent);
        }
        xml.writeEndElement(); // extentinfo
    }

    xml.writeEndElement(); // file
}

void IndexWriter::writeExtent(QXmlStreamWriter &xml, const LtfsExtent &extent)
{
    xml.writeStartElement(ELEM_EXTENT);

    xml.writeTextElement(ELEM_PARTITION, formatPartition(extent.partition()));
    xml.writeTextElement(ELEM_STARTBLOCK, QString::number(extent.startBlock()));
    xml.writeTextElement(ELEM_FILE_OFFSET, QString::number(extent.fileOffset()));
    xml.writeTextElement(ELEM_BYTE_OFFSET, QString::number(extent.byteOffset()));
    xml.writeTextElement(ELEM_BYTE_COUNT, QString::number(extent.byteCount()));

    xml.writeEndElement(); // extent
}

void IndexWriter::writeExtendedAttribute(QXmlStreamWriter &xml, const ExtendedAttribute &attr)
{
    xml.writeStartElement(ELEM_EXTENDED_ATTRIBUTE);

    xml.writeTextElement(ELEM_EA_KEY, attr.key);
    xml.writeTextElement(ELEM_EA_VALUE, attr.value);

    xml.writeEndElement(); // extendedattribute
}

void IndexWriter::writeLocation(QXmlStreamWriter &xml, const QString &elementName, const LocationData &location)
{
    xml.writeStartElement(elementName);

    xml.writeTextElement(ELEM_PARTITION, formatPartition(location.partition));
    xml.writeTextElement(ELEM_STARTBLOCK, QString::number(location.startBlock));

    xml.writeEndElement();
}

QString IndexWriter::formatTimestamp(const QDateTime &dt)
{
    if (!dt.isValid()) {
        return formatTimestamp(QDateTime::currentDateTimeUtc());
    }

    // LTFS uses ISO 8601 format with 'Z' suffix for UTC
    // Format: 2024-01-15T12:30:45.123456Z
    QDateTime utc = dt.toUTC();

    // Get microseconds precision
    QString base = utc.toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss"));

    // Add milliseconds/microseconds
    int msec = utc.time().msec();
    if (msec > 0) {
        base += QStringLiteral(".%1").arg(msec, 3, 10, QLatin1Char('0'));
    }

    base += QLatin1Char('Z');

    return base;
}

QString IndexWriter::formatPartition(PartitionLabel partition)
{
    switch (partition) {
    case PartitionLabel::IndexPartition:
        return QStringLiteral("a");
    case PartitionLabel::DataPartition:
        return QStringLiteral("b");
    }
    return QStringLiteral("b");
}

QString IndexWriter::formatBoolean(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

} // namespace qltfs
