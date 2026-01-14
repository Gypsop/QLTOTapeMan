/**
 * QLTOTapeMan - Qt-based LTO Tape Manager
 * Size Formatter Header
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 */

#ifndef QLTFS_SIZEFORMATTER_H
#define QLTFS_SIZEFORMATTER_H

#include "../libqltfs_global.h"

#include <QString>

namespace qltfs {

/**
 * @brief Size unit enumeration
 */
enum class SizeUnit {
    Bytes,
    KB,         ///< Kilobytes (1024 bytes)
    MB,         ///< Megabytes (1024^2 bytes)
    GB,         ///< Gigabytes (1024^3 bytes)
    TB,         ///< Terabytes (1024^4 bytes)
    PB,         ///< Petabytes (1024^5 bytes)
    Auto        ///< Automatically select appropriate unit
};

/**
 * @brief Utility class for formatting file and capacity sizes
 *
 * Provides human-readable size formatting with various options.
 */
class LIBQLTFS_EXPORT SizeFormatter
{
public:
    /**
     * @brief Format size as human-readable string
     * @param bytes Size in bytes
     * @param unit Target unit (Auto for automatic selection)
     * @param precision Decimal places
     * @return Formatted string like "1.5 GB"
     */
    static QString format(uint64_t bytes, SizeUnit unit = SizeUnit::Auto, int precision = 2);

    /**
     * @brief Format size with binary prefix (KiB, MiB, etc.)
     * @param bytes Size in bytes
     * @param precision Decimal places
     * @return Formatted string like "1.5 GiB"
     */
    static QString formatBinary(uint64_t bytes, int precision = 2);

    /**
     * @brief Format size with SI prefix (kB, MB, etc. - 1000-based)
     * @param bytes Size in bytes
     * @param precision Decimal places
     * @return Formatted string like "1.5 GB"
     */
    static QString formatSI(uint64_t bytes, int precision = 2);

    /**
     * @brief Format size as exact bytes with thousands separator
     * @param bytes Size in bytes
     * @return Formatted string like "1,234,567 bytes"
     */
    static QString formatExact(uint64_t bytes);

    /**
     * @brief Format size for display in file browser
     * @param bytes Size in bytes
     * @param isDirectory True if this is a directory
     * @return Formatted string or empty for directories
     */
    static QString formatForBrowser(uint64_t bytes, bool isDirectory = false);

    /**
     * @brief Parse size string to bytes
     * @param sizeStr Size string like "1.5 GB" or "1536 MB"
     * @param ok Set to true if parsing succeeded
     * @return Size in bytes
     */
    static uint64_t parse(const QString& sizeStr, bool* ok = nullptr);

    /**
     * @brief Get unit suffix string
     * @param unit Size unit
     * @return Unit string like "KB", "MB", etc.
     */
    static QString unitSuffix(SizeUnit unit);

    /**
     * @brief Get multiplier for unit
     * @param unit Size unit
     * @return Multiplier (1024 for KB, 1048576 for MB, etc.)
     */
    static uint64_t unitMultiplier(SizeUnit unit);

    /**
     * @brief Determine appropriate unit for size
     * @param bytes Size in bytes
     * @return Best unit for display
     */
    static SizeUnit appropriateUnit(uint64_t bytes);

    /**
     * @brief Format transfer speed
     * @param bytesPerSecond Speed in bytes per second
     * @param precision Decimal places
     * @return Formatted string like "125.5 MB/s"
     */
    static QString formatSpeed(double bytesPerSecond, int precision = 1);

    /**
     * @brief Format duration in human-readable form
     * @param seconds Duration in seconds
     * @return Formatted string like "2h 30m 15s"
     */
    static QString formatDuration(int64_t seconds);

    /**
     * @brief Format remaining time estimate
     * @param remainingBytes Bytes remaining
     * @param bytesPerSecond Current speed
     * @return Formatted ETA string
     */
    static QString formatETA(uint64_t remainingBytes, double bytesPerSecond);

private:
    SizeFormatter() = default; // Static class
};

} // namespace qltfs

#endif // QLTFS_SIZEFORMATTER_H
