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

#include "IndexParser.h"

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

// ============================================================================
// IndexParser Private Implementation
// ============================================================================

class IndexParser::Private
{
public:
    QString errorMessage;
    int errorLine = 0;
    int errorColumn = 0;
    bool hasError = false;

    void setError(const QString &msg, int line = 0, int column = 0)
    {
        errorMessage = msg;
        errorLine = line;
        errorColumn = column;
        hasError = true;
    }

    void clearError()
    {
        errorMessage.clear();
        errorLine = 0;
        errorColumn = 0;
        hasError = false;
    }
};

// ============================================================================
// IndexParser Implementation
// ============================================================================

IndexParser::IndexParser()
    : d(new Private)
{
}

IndexParser::~IndexParser()
{
    delete d;
}

QSharedPointer<LtfsIndex> IndexParser::parse(const QByteArray &xmlData)
{
    d->clearError();

    QXmlStreamReader xml(xmlData);
    auto index = QSharedPointer<LtfsIndex>::create();

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            if (xml.name() == ELEM_LTFSINDEX) {
                // Read version attribute
                QString version = xml.attributes().value(ATTR_VERSION).toString();
                index->setVersion(version);

                parseIndex(xml, *index);
            }
        }
    }

    if (xml.hasError()) {
        d->setError(xml.errorString(),
                   static_cast<int>(xml.lineNumber()),
                   static_cast<int>(xml.columnNumber()));
        return nullptr;
    }

    return index;
}

QSharedPointer<LtfsIndex> IndexParser::parseFile(const QString &filePath)
{
    d->clearError();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        d->setError(QStringLiteral("Failed to open file: %1").arg(file.errorString()));
        return nullptr;
    }

    QByteArray data = file.readAll();
    return parse(data);
}

QString IndexParser::errorMessage() const
{
    return d->errorMessage;
}

int IndexParser::errorLine() const
{
    return d->errorLine;
}

int IndexParser::errorColumn() const
{
    return d->errorColumn;
}

bool IndexParser::hasError() const
{
    return d->hasError;
}

void IndexParser::parseIndex(QXmlStreamReader &xml, LtfsIndex &index)
{
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::EndElement) {
            if (xml.name() == ELEM_LTFSINDEX) {
                return;
            }
        }

        if (token == QXmlStreamReader::StartElement) {
            QString name = xml.name().toString();

            if (name == ELEM_CREATOR) {
                index.setCreator(readElementText(xml));
            } else if (name == ELEM_VOLUME_UUID) {
                index.setVolumeUuid(QUuid::fromString(readElementText(xml)));
            } else if (name == ELEM_GENERATION) {
                index.setGenerationNumber(readElementText(xml).toULongLong());
            } else if (name == ELEM_UPDATE_TIME) {
                index.setUpdateTime(parseTimestamp(readElementText(xml)));
            } else if (name == ELEM_LOCATION) {
                LocationData location;
                parseLocation(xml, location);
                index.setSelfLocation(location);
            } else if (name == ELEM_PREVIOUS_GENERATION) {
                LocationData location;
                parseLocation(xml, location);
                index.setPreviousGenerationLocation(location);
            } else if (name == ELEM_ALLOW_POLICY_UPDATE) {
                index.setAllowPolicyUpdate(readElementText(xml).toLower() == QLatin1String("true"));
            } else if (name == ELEM_HIGHEST_FILE_UID) {
                index.setHighestFileUid(readElementText(xml).toULongLong());
            } else if (name == ELEM_DIRECTORY) {
                LtfsDirectory rootDir;
                parseDirectory(xml, rootDir);
                index.setRootDirectory(rootDir);
            }
        }
    }
}

void IndexParser::parseDirectory(QXmlStreamReader &xml, LtfsDirectory &directory)
{
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::EndElement) {
            if (xml.name() == ELEM_DIRECTORY) {
                return;
            }
        }

        if (token == QXmlStreamReader::StartElement) {
            QString name = xml.name().toString();

            if (name == ELEM_NAME) {
                directory.setName(readElementText(xml));
            } else if (name == ELEM_READONLY) {
                directory.setReadonly(readElementText(xml).toLower() == QLatin1String("true"));
            } else if (name == ELEM_CREATION_TIME) {
                directory.setCreationTime(parseTimestamp(readElementText(xml)));
            } else if (name == ELEM_CHANGE_TIME) {
                directory.setChangeTime(parseTimestamp(readElementText(xml)));
            } else if (name == ELEM_MODIFY_TIME) {
                directory.setModifyTime(parseTimestamp(readElementText(xml)));
            } else if (name == ELEM_ACCESS_TIME) {
                directory.setAccessTime(parseTimestamp(readElementText(xml)));
            } else if (name == ELEM_BACKUP_TIME) {
                directory.setBackupTime(parseTimestamp(readElementText(xml)));
            } else if (name == ELEM_UID) {
                directory.setUid(readElementText(xml).toULongLong());
            } else if (name == ELEM_FILE) {
                LtfsFile file;
                parseFile(xml, file);
                directory.addFile(file);
            } else if (name == ELEM_DIRECTORY) {
                LtfsDirectory subdir;
                parseDirectory(xml, subdir);
                directory.addSubdirectory(subdir);
            } else if (name == ELEM_EXTENDED_ATTRIBUTE) {
                ExtendedAttribute attr;
                parseExtendedAttribute(xml, attr);
                QList<ExtendedAttribute> attrs = directory.extendedAttributes();
                attrs.append(attr);
                directory.setExtendedAttributes(attrs);
            }
        }
    }
}

void IndexParser::parseFile(QXmlStreamReader &xml, LtfsFile &file)
{
    QList<LtfsExtent> extents;

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::EndElement) {
            if (xml.name() == ELEM_FILE) {
                file.setExtentInfo(extents);
                return;
            }
        }

        if (token == QXmlStreamReader::StartElement) {
            QString name = xml.name().toString();

            if (name == ELEM_NAME) {
                file.setName(readElementText(xml));
            } else if (name == ELEM_READONLY) {
                file.setReadonly(readElementText(xml).toLower() == QLatin1String("true"));
            } else if (name == ELEM_CREATION_TIME) {
                file.setCreationTime(parseTimestamp(readElementText(xml)));
            } else if (name == ELEM_CHANGE_TIME) {
                file.setChangeTime(parseTimestamp(readElementText(xml)));
            } else if (name == ELEM_MODIFY_TIME) {
                file.setModifyTime(parseTimestamp(readElementText(xml)));
            } else if (name == ELEM_ACCESS_TIME) {
                file.setAccessTime(parseTimestamp(readElementText(xml)));
            } else if (name == ELEM_BACKUP_TIME) {
                file.setBackupTime(parseTimestamp(readElementText(xml)));
            } else if (name == ELEM_LENGTH) {
                file.setLength(readElementText(xml).toLongLong());
            } else if (name == ELEM_UID) {
                file.setUid(readElementText(xml).toULongLong());
            } else if (name == ELEM_EXTENT_INFO) {
                // Container element, extents are parsed within
            } else if (name == ELEM_EXTENT) {
                LtfsExtent extent;
                parseExtent(xml, extent);
                extents.append(extent);
            } else if (name == ELEM_EXTENDED_ATTRIBUTE) {
                ExtendedAttribute attr;
                parseExtendedAttribute(xml, attr);
                QList<ExtendedAttribute> attrs = file.extendedAttributes();
                attrs.append(attr);
                file.setExtendedAttributes(attrs);
            }
        }
    }
}

void IndexParser::parseExtent(QXmlStreamReader &xml, LtfsExtent &extent)
{
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::EndElement) {
            if (xml.name() == ELEM_EXTENT) {
                return;
            }
        }

        if (token == QXmlStreamReader::StartElement) {
            QString name = xml.name().toString();

            if (name == ELEM_PARTITION) {
                extent.setPartition(parsePartition(readElementText(xml)));
            } else if (name == ELEM_STARTBLOCK) {
                extent.setStartBlock(readElementText(xml).toULongLong());
            } else if (name == ELEM_FILE_OFFSET) {
                extent.setFileOffset(readElementText(xml).toLongLong());
            } else if (name == ELEM_BYTE_OFFSET) {
                extent.setByteOffset(readElementText(xml).toLongLong());
            } else if (name == ELEM_BYTE_COUNT) {
                extent.setByteCount(readElementText(xml).toLongLong());
            }
        }
    }
}

void IndexParser::parseExtendedAttribute(QXmlStreamReader &xml, ExtendedAttribute &attr)
{
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::EndElement) {
            if (xml.name() == ELEM_EXTENDED_ATTRIBUTE) {
                return;
            }
        }

        if (token == QXmlStreamReader::StartElement) {
            QString name = xml.name().toString();

            if (name == ELEM_EA_KEY) {
                attr.key = readElementText(xml);
            } else if (name == ELEM_EA_VALUE) {
                attr.value = readElementText(xml);
            }
        }
    }
}

void IndexParser::parseLocation(QXmlStreamReader &xml, LocationData &location)
{
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::EndElement) {
            if (xml.name() == ELEM_LOCATION ||
                xml.name() == ELEM_PREVIOUS_GENERATION) {
                return;
            }
        }

        if (token == QXmlStreamReader::StartElement) {
            QString name = xml.name().toString();

            if (name == ELEM_PARTITION) {
                location.partition = parsePartition(readElementText(xml));
            } else if (name == ELEM_STARTBLOCK) {
                location.startBlock = readElementText(xml).toULongLong();
            }
        }
    }
}

QString IndexParser::readElementText(QXmlStreamReader &xml)
{
    return xml.readElementText(QXmlStreamReader::SkipChildElements);
}

QDateTime IndexParser::parseTimestamp(const QString &text)
{
    // LTFS uses ISO 8601 format: 2024-01-15T12:30:45.123456Z
    // Try various formats

    // Full precision with microseconds
    QDateTime dt = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (dt.isValid()) {
        return dt;
    }

    // Standard ISO format
    dt = QDateTime::fromString(text, Qt::ISODate);
    if (dt.isValid()) {
        return dt;
    }

    // Try without timezone
    QString modified = text;
    if (modified.endsWith(QLatin1Char('Z'))) {
        modified.chop(1);
    }
    dt = QDateTime::fromString(modified, QStringLiteral("yyyy-MM-ddTHH:mm:ss.zzz"));
    if (dt.isValid()) {
        dt.setTimeSpec(Qt::UTC);
        return dt;
    }

    dt = QDateTime::fromString(modified, QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
    if (dt.isValid()) {
        dt.setTimeSpec(Qt::UTC);
        return dt;
    }

    qWarning() << "Failed to parse timestamp:" << text;
    return QDateTime::currentDateTimeUtc();
}

PartitionLabel IndexParser::parsePartition(const QString &text)
{
    QString lower = text.toLower().trimmed();
    if (lower == QLatin1String("a") || lower == QLatin1String("0")) {
        return PartitionLabel::IndexPartition;
    } else if (lower == QLatin1String("b") || lower == QLatin1String("1")) {
        return PartitionLabel::DataPartition;
    }
    return PartitionLabel::DataPartition;  // Default
}

} // namespace qltfs
