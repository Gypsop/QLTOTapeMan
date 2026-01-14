/*
 * QLTOTapeMan - Qt-based LTO Tape Manager
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LTFSUTILITY_H
#define LTFSUTILITY_H

#include <QString>
#include <QByteArray>
#include <QDateTime>
#include <QStringList>
#include <QRegularExpression>

namespace qltfs {
namespace util {

/**
 * @brief LtfsUtility - Collection of LTFS-related utility functions
 *
 * This class provides static utility functions for:
 * - Size formatting (bytes to human-readable)
 * - Time formatting
 * - LTFS-specific string operations
 * - Barcode validation
 * - Path utilities
 */
class LtfsUtility
{
public:
    /**
     * @brief Format bytes to human-readable string
     * @param bytes Number of bytes
     * @param useBinary Use binary units (KiB, MiB) vs SI (KB, MB)
     * @param precision Decimal precision
     * @return Formatted string like "1.50 GiB" or "1.50 GB"
     */
    static QString formatSize(qint64 bytes, bool useBinary = true, int precision = 2);

    /**
     * @brief Parse human-readable size string to bytes
     * @param sizeStr Size string like "1.5 GiB", "500 MB"
     * @return Size in bytes, or -1 on parse error
     */
    static qint64 parseSize(const QString &sizeStr);

    /**
     * @brief Format speed to human-readable string
     * @param bytesPerSec Speed in bytes per second
     * @param useBinary Use binary units
     * @return Formatted string like "150.25 MiB/s"
     */
    static QString formatSpeed(double bytesPerSec, bool useBinary = true);

    /**
     * @brief Format duration to human-readable string
     * @param seconds Duration in seconds
     * @param showMillis Include milliseconds
     * @return Formatted string like "2:35:10" or "35:10"
     */
    static QString formatDuration(qint64 seconds, bool showMillis = false);

    /**
     * @brief Format duration in milliseconds
     * @param milliseconds Duration in milliseconds
     * @return Formatted string with milliseconds precision
     */
    static QString formatDurationMs(qint64 milliseconds);

    /**
     * @brief Format a QDateTime to LTFS index format
     * @param dateTime The datetime to format
     * @return ISO 8601 formatted string with timezone
     */
    static QString formatLtfsDateTime(const QDateTime &dateTime);

    /**
     * @brief Parse LTFS index datetime string
     * @param dateTimeStr ISO 8601 datetime string
     * @return Parsed QDateTime, or invalid datetime on error
     */
    static QDateTime parseLtfsDateTime(const QString &dateTimeStr);

    /**
     * @brief Validate a tape barcode
     * @param barcode The barcode string to validate
     * @return True if valid LTO barcode format
     */
    static bool isValidBarcode(const QString &barcode);

    /**
     * @brief Extract LTO generation from barcode
     * @param barcode The tape barcode
     * @return LTO generation number (5, 6, 7, 8, 9, etc.) or 0 if invalid
     */
    static int getLtoGenerationFromBarcode(const QString &barcode);

    /**
     * @brief Get media type string from LTO generation
     * @param generation LTO generation number
     * @return Media type string like "LTO-8"
     */
    static QString getLtoMediaTypeString(int generation);

    /**
     * @brief Get nominal capacity for LTO generation
     * @param generation LTO generation number
     * @param compressed Return compressed capacity if true
     * @return Capacity in bytes
     */
    static qint64 getLtoNominalCapacity(int generation, bool compressed = false);

    /**
     * @brief Sanitize filename for LTFS
     * @param filename Original filename
     * @return Sanitized filename safe for LTFS
     */
    static QString sanitizeLtfsFilename(const QString &filename);

    /**
     * @brief Check if a filename is valid for LTFS
     * @param filename Filename to check
     * @return True if filename is valid
     */
    static bool isValidLtfsFilename(const QString &filename);

    /**
     * @brief Normalize path for LTFS (forward slashes)
     * @param path Original path
     * @return Normalized path with forward slashes
     */
    static QString normalizeLtfsPath(const QString &path);

    /**
     * @brief Get file extension
     * @param filename Filename or path
     * @return Extension without dot, or empty string
     */
    static QString getFileExtension(const QString &filename);

    /**
     * @brief Get filename without extension
     * @param filename Filename or path
     * @return Filename without extension
     */
    static QString getFileBaseName(const QString &filename);

    /**
     * @brief Generate unique name by appending number
     * @param baseName Base filename
     * @param existingNames List of existing names to avoid
     * @return Unique name like "file (2).txt"
     */
    static QString generateUniqueName(const QString &baseName, 
                                       const QStringList &existingNames);

    /**
     * @brief Calculate block count for file size
     * @param fileSize File size in bytes
     * @param blockSize Block size in bytes (default 512KB)
     * @return Number of blocks needed
     */
    static quint64 calculateBlockCount(qint64 fileSize, 
                                        quint32 blockSize = 524288);

    /**
     * @brief Generate UUID in LTFS format
     * @return UUID string like "12345678-1234-1234-1234-123456789abc"
     */
    static QString generateUuid();

    /**
     * @brief Calculate CRC32 checksum
     * @param data Data to checksum
     * @return CRC32 value
     */
    static quint32 calculateCrc32(const QByteArray &data);

    /**
     * @brief Verify LTFS index XML signature
     * @param xmlData The XML data to verify
     * @return True if valid LTFS index signature
     */
    static bool verifyLtfsIndexSignature(const QByteArray &xmlData);

    /**
     * @brief Get creator string for LTFS index
     * @return Creator string like "QLTOTapeMan 1.0.0"
     */
    static QString getCreatorString();

    /**
     * @brief Convert block address to string
     * @param partition Partition number
     * @param block Block number
     * @return String like "P1:B12345"
     */
    static QString blockAddressToString(quint8 partition, quint64 block);

    /**
     * @brief Parse block address string
     * @param addressStr String like "P1:B12345"
     * @param partition Output partition number
     * @param block Output block number
     * @return True if parsed successfully
     */
    static bool parseBlockAddress(const QString &addressStr, 
                                   quint8 &partition, quint64 &block);

private:
    // No instantiation
    LtfsUtility() = delete;
};

/**
 * @brief ErrorHandler - Centralized error handling and logging
 */
class ErrorHandler
{
public:
    /**
     * @brief Error severity levels
     */
    enum class Severity {
        Debug,
        Info,
        Warning,
        Error,
        Critical
    };

    /**
     * @brief Error categories
     */
    enum class Category {
        General,
        Device,
        IO,
        Index,
        Filesystem,
        Network,
        Configuration
    };

    /**
     * @brief Error information structure
     */
    struct ErrorInfo {
        Severity severity;
        Category category;
        int code;
        QString message;
        QString details;
        QString source;
        QDateTime timestamp;
    };

    /**
     * @brief Get singleton instance
     * @return Reference to the error handler instance
     */
    static ErrorHandler &instance();

    /**
     * @brief Log an error
     * @param severity Error severity
     * @param category Error category
     * @param code Error code
     * @param message Error message
     * @param details Additional details (optional)
     * @param source Source location (optional)
     */
    void logError(Severity severity, Category category, int code,
                  const QString &message, const QString &details = QString(),
                  const QString &source = QString());

    /**
     * @brief Convenience method for debug logging
     */
    void debug(const QString &message, const QString &details = QString());

    /**
     * @brief Convenience method for info logging
     */
    void info(const QString &message, const QString &details = QString());

    /**
     * @brief Convenience method for warning logging
     */
    void warning(const QString &message, const QString &details = QString());

    /**
     * @brief Convenience method for error logging
     */
    void error(const QString &message, const QString &details = QString());

    /**
     * @brief Convenience method for critical error logging
     */
    void critical(const QString &message, const QString &details = QString());

    /**
     * @brief Get last error
     * @return Most recent error info
     */
    ErrorInfo lastError() const { return m_lastError; }

    /**
     * @brief Get error history
     * @param maxCount Maximum number of errors to return
     * @return List of recent errors
     */
    QList<ErrorInfo> errorHistory(int maxCount = 100) const;

    /**
     * @brief Clear error history
     */
    void clearHistory();

    /**
     * @brief Set log file path
     * @param path Path to log file
     */
    void setLogFile(const QString &path);

    /**
     * @brief Enable/disable console output
     */
    void setConsoleOutput(bool enable) { m_consoleOutput = enable; }

    /**
     * @brief Enable/disable file output
     */
    void setFileOutput(bool enable) { m_fileOutput = enable; }

    /**
     * @brief Set minimum severity for logging
     */
    void setMinimumSeverity(Severity severity) { m_minSeverity = severity; }

    /**
     * @brief Convert severity to string
     */
    static QString severityToString(Severity severity);

    /**
     * @brief Convert category to string
     */
    static QString categoryToString(Category category);

private:
    ErrorHandler();
    ~ErrorHandler();

    // No copy
    ErrorHandler(const ErrorHandler &) = delete;
    ErrorHandler &operator=(const ErrorHandler &) = delete;

    void writeToFile(const ErrorInfo &error);
    void writeToConsole(const ErrorInfo &error);

    ErrorInfo m_lastError;
    QList<ErrorInfo> m_history;
    int m_maxHistorySize;
    QString m_logFilePath;
    bool m_consoleOutput;
    bool m_fileOutput;
    Severity m_minSeverity;
};

} // namespace util
} // namespace qltfs

// Convenience macros for logging
#define QLTFS_LOG_DEBUG(msg) \
    qltfs::util::ErrorHandler::instance().debug(msg)
#define QLTFS_LOG_INFO(msg) \
    qltfs::util::ErrorHandler::instance().info(msg)
#define QLTFS_LOG_WARNING(msg) \
    qltfs::util::ErrorHandler::instance().warning(msg)
#define QLTFS_LOG_ERROR(msg) \
    qltfs::util::ErrorHandler::instance().error(msg)
#define QLTFS_LOG_CRITICAL(msg) \
    qltfs::util::ErrorHandler::instance().critical(msg)

#endif // LTFSUTILITY_H
