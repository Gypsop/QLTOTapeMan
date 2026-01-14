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

#pragma once

#include "libqltfs_global.h"
#include "core/LtfsTypes.h"
#include "core/LtfsIndex.h"

#include <QString>
#include <QByteArray>
#include <QXmlStreamReader>
#include <QSharedPointer>

namespace qltfs {

/**
 * @brief Parser for LTFS index XML format
 *
 * Parses LTFS index XML documents according to the LTFS specification.
 * Supports both reading from files and parsing from byte arrays.
 */
class LIBQLTFS_EXPORT IndexParser
{
public:
    IndexParser();
    ~IndexParser();

    // Disable copy
    IndexParser(const IndexParser &) = delete;
    IndexParser &operator=(const IndexParser &) = delete;

    /**
     * @brief Parse index from XML string/data
     * @param xmlData Raw XML data
     * @return Parsed index, or null on error
     */
    QSharedPointer<LtfsIndex> parse(const QByteArray &xmlData);

    /**
     * @brief Parse index from file
     * @param filePath Path to XML file
     * @return Parsed index, or null on error
     */
    QSharedPointer<LtfsIndex> parseFile(const QString &filePath);

    /**
     * @brief Get last error message
     */
    QString errorMessage() const;

    /**
     * @brief Get line number of last error
     */
    int errorLine() const;

    /**
     * @brief Get column of last error
     */
    int errorColumn() const;

    /**
     * @brief Check if last parse was successful
     */
    bool hasError() const;

private:
    class Private;
    Private *d;

    void parseIndex(QXmlStreamReader &xml, LtfsIndex &index);
    void parseDirectory(QXmlStreamReader &xml, LtfsDirectory &directory);
    void parseFile(QXmlStreamReader &xml, LtfsFile &file);
    void parseExtent(QXmlStreamReader &xml, LtfsExtent &extent);
    void parseExtendedAttribute(QXmlStreamReader &xml, ExtendedAttribute &attr);
    void parseLocation(QXmlStreamReader &xml, LocationData &location);
    QString readElementText(QXmlStreamReader &xml);
    QDateTime parseTimestamp(const QString &text);
    PartitionLabel parsePartition(const QString &text);
};

} // namespace qltfs
