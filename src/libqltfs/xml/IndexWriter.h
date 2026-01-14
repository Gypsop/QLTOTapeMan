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
#include <QXmlStreamWriter>
#include <QSharedPointer>

namespace qltfs {

/**
 * @brief Writer for LTFS index XML format
 *
 * Generates LTFS index XML documents according to the LTFS specification.
 */
class LIBQLTFS_EXPORT IndexWriter
{
public:
    IndexWriter();
    ~IndexWriter();

    // Disable copy
    IndexWriter(const IndexWriter &) = delete;
    IndexWriter &operator=(const IndexWriter &) = delete;

    /**
     * @brief Set whether to format output with indentation
     * @param formatted true for human-readable output
     */
    void setFormatted(bool formatted);

    /**
     * @brief Check if formatted output is enabled
     */
    bool isFormatted() const;

    /**
     * @brief Generate XML from index
     * @param index Index to serialize
     * @return XML data
     */
    QByteArray write(const LtfsIndex &index);

    /**
     * @brief Generate XML from index pointer
     */
    QByteArray write(const QSharedPointer<LtfsIndex> &index);

    /**
     * @brief Write index to file
     * @param index Index to write
     * @param filePath Output file path
     * @return true if successful
     */
    bool writeFile(const LtfsIndex &index, const QString &filePath);

    /**
     * @brief Write index pointer to file
     */
    bool writeFile(const QSharedPointer<LtfsIndex> &index, const QString &filePath);

    /**
     * @brief Get last error message
     */
    QString errorMessage() const;

private:
    class Private;
    Private *d;

    void writeIndex(QXmlStreamWriter &xml, const LtfsIndex &index);
    void writeDirectory(QXmlStreamWriter &xml, const LtfsDirectory &directory);
    void writeFile(QXmlStreamWriter &xml, const LtfsFile &file);
    void writeExtent(QXmlStreamWriter &xml, const LtfsExtent &extent);
    void writeExtendedAttribute(QXmlStreamWriter &xml, const ExtendedAttribute &attr);
    void writeLocation(QXmlStreamWriter &xml, const QString &elementName, const LocationData &location);
    QString formatTimestamp(const QDateTime &dt);
    QString formatPartition(PartitionLabel partition);
    QString formatBoolean(bool value);
};

} // namespace qltfs
