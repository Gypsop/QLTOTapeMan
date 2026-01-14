/**
 * QLTOTapeMan - Qt-based LTO Tape Manager
 * Size Formatter Implementation
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 */

#include "SizeFormatter.h"

#include <QLocale>
#include <QRegularExpression>

namespace qltfs {

// Binary multipliers (1024-based)
static constexpr uint64_t KB = 1024ULL;
static constexpr uint64_t MB = 1024ULL * 1024ULL;
static constexpr uint64_t GB = 1024ULL * 1024ULL * 1024ULL;
static constexpr uint64_t TB = 1024ULL * 1024ULL * 1024ULL * 1024ULL;
static constexpr uint64_t PB = 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL;

// SI multipliers (1000-based)
static constexpr uint64_t SI_KB = 1000ULL;
static constexpr uint64_t SI_MB = 1000ULL * 1000ULL;
static constexpr uint64_t SI_GB = 1000ULL * 1000ULL * 1000ULL;
static constexpr uint64_t SI_TB = 1000ULL * 1000ULL * 1000ULL * 1000ULL;
static constexpr uint64_t SI_PB = 1000ULL * 1000ULL * 1000ULL * 1000ULL * 1000ULL;

QString SizeFormatter::format(uint64_t bytes, SizeUnit unit, int precision)
{
    if (unit == SizeUnit::Auto) {
        unit = appropriateUnit(bytes);
    }

    double value;
    QString suffix;

    switch (unit) {
    case SizeUnit::Bytes:
        return QStringLiteral("%1 bytes").arg(bytes);
    case SizeUnit::KB:
        value = static_cast<double>(bytes) / KB;
        suffix = QStringLiteral("KB");
        break;
    case SizeUnit::MB:
        value = static_cast<double>(bytes) / MB;
        suffix = QStringLiteral("MB");
        break;
    case SizeUnit::GB:
        value = static_cast<double>(bytes) / GB;
        suffix = QStringLiteral("GB");
        break;
    case SizeUnit::TB:
        value = static_cast<double>(bytes) / TB;
        suffix = QStringLiteral("TB");
        break;
    case SizeUnit::PB:
        value = static_cast<double>(bytes) / PB;
        suffix = QStringLiteral("PB");
        break;
    default:
        return formatExact(bytes);
    }

    return QStringLiteral("%1 %2").arg(value, 0, 'f', precision).arg(suffix);
}

QString SizeFormatter::formatBinary(uint64_t bytes, int precision)
{
    if (bytes < KB) {
        return QStringLiteral("%1 bytes").arg(bytes);
    } else if (bytes < MB) {
        return QStringLiteral("%1 KiB").arg(static_cast<double>(bytes) / KB, 0, 'f', precision);
    } else if (bytes < GB) {
        return QStringLiteral("%1 MiB").arg(static_cast<double>(bytes) / MB, 0, 'f', precision);
    } else if (bytes < TB) {
        return QStringLiteral("%1 GiB").arg(static_cast<double>(bytes) / GB, 0, 'f', precision);
    } else if (bytes < PB) {
        return QStringLiteral("%1 TiB").arg(static_cast<double>(bytes) / TB, 0, 'f', precision);
    } else {
        return QStringLiteral("%1 PiB").arg(static_cast<double>(bytes) / PB, 0, 'f', precision);
    }
}

QString SizeFormatter::formatSI(uint64_t bytes, int precision)
{
    if (bytes < SI_KB) {
        return QStringLiteral("%1 bytes").arg(bytes);
    } else if (bytes < SI_MB) {
        return QStringLiteral("%1 kB").arg(static_cast<double>(bytes) / SI_KB, 0, 'f', precision);
    } else if (bytes < SI_GB) {
        return QStringLiteral("%1 MB").arg(static_cast<double>(bytes) / SI_MB, 0, 'f', precision);
    } else if (bytes < SI_TB) {
        return QStringLiteral("%1 GB").arg(static_cast<double>(bytes) / SI_GB, 0, 'f', precision);
    } else if (bytes < SI_PB) {
        return QStringLiteral("%1 TB").arg(static_cast<double>(bytes) / SI_TB, 0, 'f', precision);
    } else {
        return QStringLiteral("%1 PB").arg(static_cast<double>(bytes) / SI_PB, 0, 'f', precision);
    }
}

QString SizeFormatter::formatExact(uint64_t bytes)
{
    QLocale locale;
    return QStringLiteral("%1 bytes").arg(locale.toString(static_cast<qulonglong>(bytes)));
}

QString SizeFormatter::formatForBrowser(uint64_t bytes, bool isDirectory)
{
    if (isDirectory) {
        return QString(); // No size for directories
    }
    
    return format(bytes, SizeUnit::Auto, 1);
}

uint64_t SizeFormatter::parse(const QString& sizeStr, bool* ok)
{
    if (ok) *ok = false;
    
    QString str = sizeStr.trimmed().toUpper();
    if (str.isEmpty()) {
        return 0;
    }

    // Match number with optional unit
    static QRegularExpression regex(QStringLiteral(R"(^([\d.,]+)\s*([KMGTP]?I?B?)$)"));
    QRegularExpressionMatch match = regex.match(str);
    
    if (!match.hasMatch()) {
        return 0;
    }

    QString numberStr = match.captured(1).replace(QLatin1Char(','), QLatin1Char('.'));
    QString unitStr = match.captured(2);
    
    bool parseOk = false;
    double value = numberStr.toDouble(&parseOk);
    if (!parseOk) {
        return 0;
    }

    uint64_t multiplier = 1;
    
    if (unitStr.isEmpty() || unitStr == QStringLiteral("B") || unitStr == QStringLiteral("BYTES")) {
        multiplier = 1;
    } else if (unitStr == QStringLiteral("K") || unitStr == QStringLiteral("KB") || unitStr == QStringLiteral("KIB")) {
        multiplier = KB;
    } else if (unitStr == QStringLiteral("M") || unitStr == QStringLiteral("MB") || unitStr == QStringLiteral("MIB")) {
        multiplier = MB;
    } else if (unitStr == QStringLiteral("G") || unitStr == QStringLiteral("GB") || unitStr == QStringLiteral("GIB")) {
        multiplier = GB;
    } else if (unitStr == QStringLiteral("T") || unitStr == QStringLiteral("TB") || unitStr == QStringLiteral("TIB")) {
        multiplier = TB;
    } else if (unitStr == QStringLiteral("P") || unitStr == QStringLiteral("PB") || unitStr == QStringLiteral("PIB")) {
        multiplier = PB;
    } else {
        return 0;
    }

    if (ok) *ok = true;
    return static_cast<uint64_t>(value * multiplier);
}

QString SizeFormatter::unitSuffix(SizeUnit unit)
{
    switch (unit) {
    case SizeUnit::Bytes: return QStringLiteral("bytes");
    case SizeUnit::KB:    return QStringLiteral("KB");
    case SizeUnit::MB:    return QStringLiteral("MB");
    case SizeUnit::GB:    return QStringLiteral("GB");
    case SizeUnit::TB:    return QStringLiteral("TB");
    case SizeUnit::PB:    return QStringLiteral("PB");
    case SizeUnit::Auto:  return QString();
    }
    return QString();
}

uint64_t SizeFormatter::unitMultiplier(SizeUnit unit)
{
    switch (unit) {
    case SizeUnit::Bytes: return 1;
    case SizeUnit::KB:    return KB;
    case SizeUnit::MB:    return MB;
    case SizeUnit::GB:    return GB;
    case SizeUnit::TB:    return TB;
    case SizeUnit::PB:    return PB;
    case SizeUnit::Auto:  return 1;
    }
    return 1;
}

SizeUnit SizeFormatter::appropriateUnit(uint64_t bytes)
{
    if (bytes < KB) {
        return SizeUnit::Bytes;
    } else if (bytes < MB) {
        return SizeUnit::KB;
    } else if (bytes < GB) {
        return SizeUnit::MB;
    } else if (bytes < TB) {
        return SizeUnit::GB;
    } else if (bytes < PB) {
        return SizeUnit::TB;
    } else {
        return SizeUnit::PB;
    }
}

QString SizeFormatter::formatSpeed(double bytesPerSecond, int precision)
{
    if (bytesPerSecond < 0) {
        return QStringLiteral("-- B/s");
    }
    
    QString sizeStr = format(static_cast<uint64_t>(bytesPerSecond), SizeUnit::Auto, precision);
    // Remove " bytes" or similar and add "/s"
    sizeStr.replace(QStringLiteral(" bytes"), QStringLiteral(" B"));
    return sizeStr + QStringLiteral("/s");
}

QString SizeFormatter::formatDuration(int64_t seconds)
{
    if (seconds < 0) {
        return QStringLiteral("--");
    }
    
    if (seconds < 60) {
        return QStringLiteral("%1s").arg(seconds);
    }
    
    int64_t minutes = seconds / 60;
    int64_t secs = seconds % 60;
    
    if (minutes < 60) {
        return QStringLiteral("%1m %2s").arg(minutes).arg(secs);
    }
    
    int64_t hours = minutes / 60;
    minutes = minutes % 60;
    
    if (hours < 24) {
        return QStringLiteral("%1h %2m %3s").arg(hours).arg(minutes).arg(secs);
    }
    
    int64_t days = hours / 24;
    hours = hours % 24;
    
    return QStringLiteral("%1d %2h %3m").arg(days).arg(hours).arg(minutes);
}

QString SizeFormatter::formatETA(uint64_t remainingBytes, double bytesPerSecond)
{
    if (bytesPerSecond <= 0) {
        return QStringLiteral("Calculating...");
    }
    
    int64_t seconds = static_cast<int64_t>(remainingBytes / bytesPerSecond);
    return formatDuration(seconds);
}

} // namespace qltfs
