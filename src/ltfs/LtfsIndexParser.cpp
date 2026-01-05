#include "LtfsIndexParser.h"
#include <QDebug>

LtfsIndexParser::LtfsIndexParser()
{
}

QString LtfsIndexParser::errorString() const
{
    return m_errorString;
}

bool LtfsIndexParser::parse(const QString &filePath, LtfsIndex &index)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_errorString = "Could not open file: " + file.errorString();
        return false;
    }

    QXmlStreamReader xml(&file);
    
    if (xml.readNextStartElement()) {
        if (xml.name() == QStringLiteral("ltfsindex")) {
            readLtfsIndex(xml, index);
        } else {
            m_errorString = "Not an LTFS index file.";
            return false;
        }
    }

    if (xml.hasError()) {
        m_errorString = xml.errorString();
        return false;
    }

    return true;
}

void LtfsIndexParser::readLtfsIndex(QXmlStreamReader &xml, LtfsIndex &index)
{
    // Read attributes
    foreach(const QXmlStreamAttribute &attr, xml.attributes()) {
        if (attr.name() == QStringLiteral("volumeuuid"))
            index.volumeUuid = attr.value().toString();
        else if (attr.name() == QStringLiteral("creator"))
            index.creator = attr.value().toString();
        else if (attr.name() == QStringLiteral("generationnumber"))
            index.generationNumber = attr.value().toULongLong();
        else if (attr.name() == QStringLiteral("updatetime"))
            index.updateTime = QDateTime::fromString(attr.value().toString(), Qt::ISODate);
    }

    while (xml.readNextStartElement()) {
        if (xml.name() == QStringLiteral("directory")) {
            readDirectory(xml, index.rootDirectory);
        } else if (xml.name() == QStringLiteral("location")) {
             // Parse location info if needed
             xml.skipCurrentElement();
        } else {
            xml.skipCurrentElement();
        }
    }
}

void LtfsIndexParser::readDirectory(QXmlStreamReader &xml, LtfsDirectory &directory)
{
    // Read directory attributes
    foreach(const QXmlStreamAttribute &attr, xml.attributes()) {
        if (attr.name() == QStringLiteral("name"))
            directory.name = attr.value().toString();
    }

    while (xml.readNextStartElement()) {
        if (xml.name() == QStringLiteral("file")) {
            LtfsFile file;
            readFile(xml, file);
            directory.files.append(file);
        } else if (xml.name() == QStringLiteral("directory")) {
            LtfsDirectory subDir;
            readDirectory(xml, subDir);
            directory.subdirectories.append(subDir);
        } else if (xml.name() == QStringLiteral("xattr")) {
            readXattr(xml, directory.extendedAttributes);
        } else {
            xml.skipCurrentElement();
        }
    }
}

void LtfsIndexParser::readFile(QXmlStreamReader &xml, LtfsFile &file)
{
    foreach(const QXmlStreamAttribute &attr, xml.attributes()) {
        if (attr.name() == QStringLiteral("name"))
            file.name = attr.value().toString();
        else if (attr.name() == QStringLiteral("length"))
            file.length = attr.value().toULongLong();
        else if (attr.name() == QStringLiteral("readonly"))
            file.readonly = (attr.value() == QStringLiteral("true"));
    }

    while (xml.readNextStartElement()) {
        if (xml.name() == QStringLiteral("extent")) {
            LtfsExtent extent;
            readExtent(xml, extent);
            file.extents.append(extent);
        } else if (xml.name() == QStringLiteral("xattr")) {
            readXattr(xml, file.extendedAttributes);
            if (file.extendedAttributes.contains("user.ltfs.hash.sha1")) {
                file.sha1 = file.extendedAttributes["user.ltfs.hash.sha1"];
            }
        } else {
            xml.skipCurrentElement();
        }
    }
}

void LtfsIndexParser::readExtent(QXmlStreamReader &xml, LtfsExtent &extent)
{
    foreach(const QXmlStreamAttribute &attr, xml.attributes()) {
        if (attr.name() == QStringLiteral("startblock"))
            extent.startBlock = attr.value().toULongLong();
        else if (attr.name() == QStringLiteral("byteoffset"))
            extent.byteOffset = attr.value().toULongLong();
        else if (attr.name() == QStringLiteral("bytecount"))
            extent.byteCount = attr.value().toULongLong();
        else if (attr.name() == QStringLiteral("partition"))
            extent.partition = attr.value().toString();
        else if (attr.name() == QStringLiteral("fileoffset"))
            extent.fileOffset = attr.value().toULongLong();
    }
    xml.skipCurrentElement(); // Extent has no children
}

void LtfsIndexParser::readXattr(QXmlStreamReader &xml, QMap<QString, QString> &attributes)
{
    QString key;
    QString value;
    
    while (xml.readNextStartElement()) {
        if (xml.name() == QStringLiteral("key")) {
            key = xml.readElementText();
        } else if (xml.name() == QStringLiteral("value")) {
            value = xml.readElementText();
        } else {
            xml.skipCurrentElement();
        }
    }
    
    if (!key.isEmpty()) {
        attributes.insert(key, value);
    }
}
